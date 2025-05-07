#include "ws.hpp"
#include "../util/logger.h"
#include "../util/pck.h"

#include <poll.h>     // para poll()
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <unordered_set>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

WebSocketServer::WebSocketServer(int port, int max_threads)
  : port_(port), server_fd_(-1), epoll_fd_(-1), running_(true) {
  for (int i = 0; i < max_threads; ++i) {
    thread_pool_.emplace_back(&WebSocketServer::worker_thread, this);
  }
}

WebSocketServer::~WebSocketServer() {
  stop();
  for (auto& thread : thread_pool_) {
    if (thread.joinable()) thread.join();
  }
}

void WebSocketServer::set_message_handler(MessageHandler handler) {
  custom_message_handler_ = handler;
}

void WebSocketServer::set_handshake_validator(HandshakeValidator validator) {
  custom_handshake_validator_ = validator;
}

void WebSocketServer::run() {
  if (!setup_server_socket()) return;

  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ < 0) {
    logger::error("Failed to create epoll instance", __func__);
    stop();
    return;
  }

  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = server_fd_;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &event) < 0) {
    logger::error("Failed to add server socket to epoll", __func__);
    stop();
    return;
  }

  logger::info("WebSocket server is running", __func__);
  handle_events();
}

void WebSocketServer::stop() {
  running_ = false;
  close(server_fd_);
  close(epoll_fd_);

  {
    std::lock_guard<std::mutex> lock(close_sockets_mutex_);
    closed_sockets_.clear();  // Limpiar al detener el servidor
  }

  task_cv_.notify_all();
  for (auto& thread : thread_pool_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

bool WebSocketServer::setup_server_socket() {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    logger::error("Failed to create server socket", __func__);
    return false;
  }

  int opt = 1;
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    logger::error("Failed to set socket options", __func__);
    close(server_fd_);
    return false;
  }

  struct sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port_);

  if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
    logger::error("Failed to bind server socket", __func__);
    close(server_fd_);
    return false;
  }

  if (listen(server_fd_, SOMAXCONN) < 0) {
    logger::error("Failed to listen on server socket", __func__);
    close(server_fd_);
    return false;
  }

  logger::info("Server socket setup successfully", __func__);
  return true;
}

void WebSocketServer::handle_events() {
  struct epoll_event events[10];
  while (running_) {
    int num_events = epoll_wait(epoll_fd_, events, 10, -1);
    if (num_events < 0) {
      if (errno == EINTR) continue;  // Señal interrumpida, continuar
      logger::error("epoll_wait failed: " + std::string(strerror(errno)), __func__);
      break;
    }

    for (int i = 0; i < num_events; ++i) {
      int fd = events[i].data.fd;
      if (fd == server_fd_) {
        int client_socket = accept(server_fd_, nullptr, nullptr);
        if (client_socket >= 0) {
          fcntl(client_socket, F_SETFL, O_NONBLOCK);

          // Remove from closed_sockets_ in case FD is reused
          {
            std::lock_guard<std::mutex> lock(close_sockets_mutex_);
            closed_sockets_.erase(client_socket);
          }
  
          if (perform_handshake(client_socket)) {
            struct epoll_event client_event;
            client_event.events = EPOLLIN;
            client_event.data.fd = client_socket;
            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_socket, &client_event);

            // Add to connected clients
            {
              std::lock_guard<std::mutex> lock(connected_clients_mutex_);
              connected_clients_.insert(client_socket);
            }
          } else {
            close(client_socket);
          }
        }
      } else {
        {
          std::lock_guard<std::mutex> lock(queue_mutex_);
          task_queue_.push(fd);
        }
        task_cv_.notify_one();  // Despierta un hilo para procesar el socket
      }
    }
  }
}

void WebSocketServer::worker_thread() {
  while (running_) {
    int client_fd = -1;

    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      task_cv_.wait(lock, [&]() { return !task_queue_.empty() || !running_; });

      if (!running_) break;

      client_fd = task_queue_.front();
      task_queue_.pop();
    }

    handle_client_read(client_fd);
  }
}

void WebSocketServer::handle_client_read(int client_socket) {
  // Only lock for the check, then unlock!
  {
    std::lock_guard<std::mutex> lock(close_sockets_mutex_);
    if (closed_sockets_.find(client_socket) != closed_sockets_.end()) {
      logger::warn("Skipping reading from closed socket (fd: " + std::to_string(client_socket) + ")", __func__);
      return;
    }
  }

  std::vector<uint8_t> buffer(4096);
  ssize_t bytes_read = recv(client_socket, buffer.data(), buffer.size(), 0);
  if (bytes_read <= 0) {
    if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }
    logger::error("Failed to read from client socket (fd: " + std::to_string(client_socket) + "): " + strerror(errno), __func__);
    close_connection(client_socket);
    return;
  }

  buffer.resize(bytes_read);
  std::string payload;
  bool frame_ok = decode_frame(buffer, payload);

  if (!frame_ok) {
    logger::info("Closing connection after failed frame decode (fd: " + std::to_string(client_socket) + ")", __func__);
    close_connection(client_socket);
    return;
  }

  if (payload.empty()) {
    return;
  }

  if (!is_valid_utf8(payload)) {
    payload = sanitize_utf8(payload);
  }

  if (custom_message_handler_) {
    logger::info("Custom message handler invoked for fd: " + std::to_string(client_socket) + " with message: " + payload, __func__);
    custom_message_handler_(client_socket, payload);
  } else {
    logger::info("Default message handler invoked for fd: " + std::to_string(client_socket) + " with message: " + payload, __func__);
    on_message(client_socket, payload);
  }
}

void WebSocketServer::handle_client_write(int client_socket, const std::string& message) {
  if (client_socket < 0) {
    logger::warn("Invalid client socket for writing", __func__);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(close_sockets_mutex_);
    if (closed_sockets_.find(client_socket) != closed_sockets_.end()) {
      logger::warn("Attempted to write to closed socket (fd: " + std::to_string(client_socket) + ")", __func__);
      return;
    }
  }

  if (message.empty()) {
    logger::warn("Attempted to send an empty message (fd: " + std::to_string(client_socket) + ")", __func__);
    return;
  }

  std::string processed_message = process_data(message);
  std::vector<uint8_t> frame = encode_frame(processed_message);
  if (frame.empty()) {
    logger::error("Failed to encode WebSocket frame", __func__);
    return;
  }

  ssize_t sent = send(client_socket, frame.data(), frame.size(), 0);
  if (sent < 0) {
    logger::error("Failed to send WebSocket frame to client (fd: " + std::to_string(client_socket) + "): " + std::string(strerror(errno)), __func__);
    close_connection(client_socket);
  } else {
    logger::info("Sent WebSocket frame to client (fd: " + std::to_string(client_socket) + "): " + processed_message, __func__);
  }
}

bool WebSocketServer::decode_frame(const std::vector<uint8_t>& frame, std::string& payload) {
  if (frame.size() < 2) {
    logger::error("Frame too short to decode", __func__);
    return false;
  }

  uint8_t opcode = frame[0] & 0x0F;
  if (opcode == 0x8) { // Close frame
    logger::info("Received close frame", __func__);
    return false;
  } else if (opcode == 0x9) { // Ping frame
    logger::info("Received ping frame", __func__);
    return true;
  } else if (opcode == 0xA) { // Pong frame
    logger::info("Received pong frame", __func__);
    return true;
  } else if (opcode != 0x1) {
    logger::error("Unsupported opcode: " + std::to_string(opcode), __func__);
    return false;
  }

  bool masked = frame[1] & 0x80;
  size_t payload_len = frame[1] & 0x7F;
  size_t offset = 2;

  if (payload_len == 126) {
    if (frame.size() < 4) {
      logger::error("Frame too short for extended payload length (126)", __func__);
      return false;
    }
    payload_len = (frame[2] << 8) | frame[3];
    offset += 2;
  } else if (payload_len == 127) {
    if (frame.size() < 10) {
      logger::error("Frame too short for extended payload length (127)", __func__);
      return false;
    }
    payload_len = 0;
    for (int i = 0; i < 8; ++i) {
      payload_len = (payload_len << 8) | frame[2 + i];
    }
    offset += 8;
  }

  if (masked) {
    if (frame.size() < offset + 4 + payload_len) {
      logger::error("Frame too short for masked payload", __func__);
      return false;
    }
    uint8_t mask[4] = {frame[offset], frame[offset + 1], frame[offset + 2], frame[offset + 3]};
    offset += 4;

    payload.resize(payload_len);
    for (size_t i = 0; i < payload_len; ++i) {
      payload[i] = frame[offset + i] ^ mask[i % 4];
    }
  } else {
    if (frame.size() < offset + payload_len) {
      logger::error("Frame too short for unmasked payload", __func__);
      return false;
    }
    payload.assign(frame.begin() + offset, frame.begin() + offset + payload_len);
  }

  logger::info("Decoded WebSocket frame: " + payload, __func__);
  return is_valid_utf8(payload);
}

void WebSocketServer::close_connection(int client_socket) {
  {
    std::lock_guard<std::mutex> lock(close_sockets_mutex_);
    if (closed_sockets_.count(client_socket)) {
        logger::warn("Attempted to close already closed socket (fd: " + std::to_string(client_socket) + ")", __func__);
        return;
    }
    closed_sockets_.insert(client_socket);
  }

  logger::info("Closing connection (fd: " + std::to_string(client_socket) + ")", __func__);

  {
    std::lock_guard<std::mutex> lock(client_mutex_);
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_socket, nullptr) < 0) {
        logger::error("Failed to remove client fd from epoll: " + std::string(strerror(errno)), __func__);
    }
    close(client_socket);
    logger::info("Closed connection (fd: " + std::to_string(client_socket) + ")", __func__);
  }

  {
    std::lock_guard<std::mutex> lock(connected_clients_mutex_);
    connected_clients_.erase(client_socket);
  }
}


std::vector<uint8_t> WebSocketServer::encode_frame(const std::string& payload) {
  std::vector<uint8_t> frame;
  frame.push_back(0x81); // FIN + text frame

  size_t payload_len = payload.size();
  if (payload_len <= 125) {
    frame.push_back(static_cast<uint8_t>(payload_len));
  } else if (payload_len <= 65535) {
    frame.push_back(126);
    frame.push_back((payload_len >> 8) & 0xFF);
    frame.push_back(payload_len & 0xFF);
  } else {
    frame.push_back(127);
    for (int i = 7; i >= 0; --i) {
      frame.push_back((payload_len >> (i * 8)) & 0xFF);
    }
  }

  frame.insert(frame.end(), payload.begin(), payload.end());
  return frame;
}

bool WebSocketServer::is_valid_utf8(const std::string& str) {
  int bytes = 0;
  for (unsigned char c : str) {
    if (bytes == 0) {
      if ((c >> 5) == 0b110) bytes = 1;
      else if ((c >> 4) == 0b1110) bytes = 2;
      else if ((c >> 3) == 0b11110) bytes = 3;
      else if ((c >> 7)) return false;
    } else {
      if ((c >> 6) != 0b10) return false;
      --bytes;
    }
  }
  return bytes == 0;
}

std::string WebSocketServer::sanitize_utf8(const std::string& str) {
  std::string sanitized;
  for (unsigned char c : str) {
    if ((c >> 7) == 0 || (c >> 5) == 0b110 || (c >> 4) == 0b1110 || (c >> 3) == 0b11110) {
      sanitized += c;
    } else {
      sanitized += '?';
    }
  }
  return sanitized;
}

std::string WebSocketServer::process_data(const std::string& data) {
  // Default implementation: return the data as-is
  return data;
}

void WebSocketServer::on_message(int client_socket, const std::string& message) {
  //logger::info("on_message called for fd: " + std::to_string(client_socket) + " with message: " + message, __func__);
  /*
  std::string response = "Echo: " + message;
  handle_client_write(client_socket, response);
  */
  {
    std::lock_guard<std::mutex> lock(response_mutex_);
    response_queue_.push(message);
  }
  response_cv_.notify_one();
}

bool WebSocketServer::validate_handshake(const std::string& request) {
  // Custom handshake validation logic
  return true;
}

bool WebSocketServer::perform_handshake(int client_socket) {
  struct pollfd pfd;
  pfd.fd = client_socket;
  pfd.events = POLLIN;

  int poll_result = poll(&pfd, 1, 5000);
  if (poll_result <= 0) {
    if (poll_result == 0) {
      logger::error("Timeout waiting ", __func__);
    } else {
      logger::error("Error en poll(): " + std::string(strerror(errno)), __func__);
    }
    return false;
  }

  std::string request;
  char buffer[2048];
  ssize_t len;
  size_t total = 0;
  while (request.find("\r\n\r\n") == std::string::npos && total < sizeof(buffer) - 1) {
    len = recv(client_socket, buffer, sizeof(buffer) - 1 - total, 0);
    if (len <= 0) {
      logger::error("Error receiving handshake request: " + std::string(strerror(errno)), __func__);
      return false;
    }
    buffer[len] = '\0';
    request.append(buffer, len);
    total += len;
  }
  logger::info("Received handshake request:\n" + request, __func__);

  if (custom_handshake_validator_ && !custom_handshake_validator_(request)) {
    logger::error("Custom handshake validation failed", __func__);
    return false;
  }

  std::string sec_websocket_key = extract_header(request, "Sec-WebSocket-Key");
  if (sec_websocket_key.empty()) {
    logger::error("Missing Sec-WebSocket-Key in handshake request", __func__);
    return false;
  }

  std::string sec_websocket_accept = compute_accept_key(sec_websocket_key);

  std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                         "Upgrade: websocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Accept: " + sec_websocket_accept + "\r\n\r\n";

  if (send(client_socket, response.c_str(), response.size(), 0) < 0) {
    logger::error("Failed to send handshake response: " + std::string(strerror(errno)), __func__);
    return false;
  }

  logger::info("WebSocket handshake completed successfully", __func__);
  return true;
}

std::string WebSocketServer::extract_header(const std::string& request, const std::string& header_name) {
  std::istringstream stream(request);
  std::string line;
  std::string prefix = header_name + ":";
  while (std::getline(stream, line)) {
    if (line.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), line.begin(),
        [](char a, char b){ return std::tolower(a) == std::tolower(b); })) {
      size_t pos = line.find(':');
      if (pos != std::string::npos) {
        size_t start = line.find_first_not_of(" \t", pos + 1);
        size_t end = line.find_last_not_of("\r\n");
        if (start != std::string::npos && end != std::string::npos && end >= start)
          return line.substr(start, end - start + 1);
      }
    }
  }
  return "";
}

std::string WebSocketServer::compute_accept_key(const std::string& sec_websocket_key) {
  std::string accept_key = sec_websocket_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(accept_key.c_str()), accept_key.size(), hash);

  BIO* bio = BIO_new(BIO_s_mem());
  BIO* b64 = BIO_new(BIO_f_base64());
  bio = BIO_push(b64, bio);
  int written = BIO_write(bio, hash, SHA_DIGEST_LENGTH);
  if (written != SHA_DIGEST_LENGTH) {
    BIO_free_all(bio);
    logger::error("BIO_write failed in compute_accept_key", __func__);
    return "";
  }
  BIO_flush(bio);

  BUF_MEM* buffer_ptr;
  BIO_get_mem_ptr(bio, &buffer_ptr);
  std::string sec_websocket_accept(buffer_ptr->data, buffer_ptr->length - 1);
  BIO_free_all(bio);

  return sec_websocket_accept;
}

void WebSocketServer::broadcast(const std::string& message) {
  logger::info("Broadcasting message to all clients", __func__);
  std::unordered_set<int> clients_copy;
  {
    std::lock_guard<std::mutex> lock(connected_clients_mutex_);
    clients_copy = connected_clients_;
  }
  logger::info("Number of clients to broadcast: " + std::to_string(clients_copy.size()), __func__);
  for (int client_socket : clients_copy) {
    logger::info("Broadcasting message to client socket: " + std::to_string(client_socket), __func__);
    handle_client_write(client_socket, message);
  }
}

std::string WebSocketServer::receive_response() {
  logger::info("Waiting for response from WebSocket client", __func__);
  std::unique_lock<std::mutex> lock(response_mutex_);
  response_cv_.wait(lock, [&]{ return !response_queue_.empty(); });
  std::string msg = response_queue_.front();
  response_queue_.pop();
  return msg;
}