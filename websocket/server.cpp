#include "server.hpp"
#include "../util/pck.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h> // Include EVP for BIO_f_base64
#include <thread> // Include thread for std::this_thread

#define MAX_EVENTS 10

WebSocketServer::WebSocketServer(int port) : port_(port), server_fd_(-1), epoll_fd_(-1) {}

WebSocketServer::~WebSocketServer() {
  if (server_fd_ != -1) close(server_fd_);
  if (epoll_fd_ != -1) close(epoll_fd_);
}

std::string WebSocketServer::_deb(const std::string& msg, const std::string& func) const {
  std::ostringstream oss;
  oss << "[WebSocketServer][" << func << "] " << msg;
  return oss.str();
}

bool WebSocketServer::setup_server_socket() {
  struct sockaddr_in address;
  int opt = 1;

  // Create server socket
  if ((server_fd_ = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    std::cerr << _deb("Failed to create socket", __func__) << std::endl;
    return false;
  }

  // Set socket options
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    std::cerr << _deb("Failed to set socket options", __func__) << std::endl;
    return false;
  }

  // Set socket to non-blocking mode
  if (fcntl(server_fd_, F_SETFL, O_NONBLOCK) < 0) {
    std::cerr << _deb("Failed to set non-blocking mode", __func__) << std::endl;
    return false;
  }

  // Bind socket to port
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port_);
  if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
    std::cerr << _deb("Failed to bind socket", __func__) << std::endl;
    return false;
  }

  // Start listening
  if (listen(server_fd_, SOMAXCONN) < 0) {
    std::cerr << _deb("Failed to listen on socket", __func__) << std::endl;
    return false;
  }

  return true;
}

void WebSocketServer::run() {
  if (!setup_server_socket()) {
    exit(EXIT_FAILURE);
  }

  // Create epoll instance
  if ((epoll_fd_ = epoll_create1(0)) < 0) {
    std::cerr << _deb("Failed to create epoll instance", __func__) << std::endl;
    exit(EXIT_FAILURE);
  }

  // Add server socket to epoll
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = server_fd_;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &event) < 0) {
    std::cerr << _deb("Failed to add server socket to epoll", __func__) << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << _deb("Server listening on port " + std::to_string(port_), __func__) << std::endl;

  // Event loop
  handle_events();
}

void WebSocketServer::handle_events() {
  struct epoll_event events[MAX_EVENTS];

  while (true) {
    int num_events = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
    if (num_events < 0) {
      std::cerr << _deb("Error in epoll_wait", __func__) << std::endl;
      break;
    }

    for (int i = 0; i < num_events; ++i) {
      int event_fd = events[i].data.fd;

      if (event_fd == server_fd_) {
        // Accept new client connection
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        int client_socket = accept(server_fd_, (struct sockaddr*)&client_address, &client_len);
        if (client_socket < 0) {
          std::cerr << _deb("Failed to accept client connection", __func__) << std::endl;
          continue;
        }

        // Set client socket to non-blocking mode
        if (fcntl(client_socket, F_SETFL, O_NONBLOCK) < 0) {
          std::cerr << _deb("Failed to set non-blocking mode for client", __func__) << std::endl;
          close(client_socket);
          continue;
        }

        // Perform WebSocket handshake
        if (!perform_handshake(client_socket)) {
          std::cerr << _deb("Handshake failed", __func__) << std::endl;
          close(client_socket);
          continue;
        }

        // Add client socket to epoll
        struct epoll_event client_event;
        client_event.events = EPOLLIN | EPOLLET; // Edge-triggered
        client_event.data.fd = client_socket;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_socket, &client_event) < 0) {
          std::cerr << _deb("Failed to add client socket to epoll", __func__) << std::endl;
          close(client_socket);
          continue;
        }

        std::cout << _deb("Accepted new client connection", __func__) << std::endl;
      } else {
        // Handle client events
        if (events[i].events & EPOLLIN) {
          handle_client_read(event_fd);
        }
      }
    }
  }
}

void WebSocketServer::handle_client_read(int client_socket) {
  char buffer[1024];
  ssize_t len = recv(client_socket, buffer, sizeof(buffer), 0);
  if (len <= 0) {
    std::cerr << _deb("Client disconnected or error reading", __func__) << std::endl;
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_socket, nullptr);
    close(client_socket);
    return;
  }

  // Process the message
  std::string message;
  if (!decode_frame(buffer, len, message)) {
    std::cerr << _deb("Error decoding WebSocket frame", __func__) << std::endl;
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_socket, nullptr);
    close(client_socket);
    return;
  }

  std::cout << _deb("Received message: " + message, __func__) << std::endl;

  // Echo the message back to the client
  handle_client_write(client_socket, message);
}

void WebSocketServer::handle_client_write(int client_socket, const std::string& message) {
  // Prepare WebSocket frame
  std::vector<uint8_t> frame;
  frame.push_back(0x81); // FIN + text frame (opcode 0x1)

  size_t payload_len = message.size();
  if (payload_len <= 125) {
    frame.push_back(payload_len); // No mask bit for server-to-client frames
  } else if (payload_len <= 65535) {
    frame.push_back(126); // Extended payload length (16-bit)
    frame.push_back((payload_len >> 8) & 0xFF);
    frame.push_back(payload_len & 0xFF);
  } else {
    frame.push_back(127); // Extended payload length (64-bit)
    for (int i = 7; i >= 0; --i) {
      frame.push_back((payload_len >> (i * 8)) & 0xFF);
    }
  }

  // Append the payload
  frame.insert(frame.end(), message.begin(), message.end());

  // Debug: Print the constructed frame
  std::cout << "[handle_client_write] Constructed frame: ";
  for (uint8_t byte : frame) {
    std::cout << std::hex << static_cast<int>(byte) << " ";
  }
  std::cout << std::dec << std::endl;

  // Send the frame
  if (send(client_socket, frame.data(), frame.size(), 0) < 0) {
    std::cerr << _deb("Error sending message to client", __func__) << std::endl;
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_socket, nullptr);
    close(client_socket);
  }
}

bool WebSocketServer::decode_frame(const char* buffer, ssize_t len, std::string& message) {
  if (len < 2) {
    std::cerr << "[decode_frame] Frame too short" << std::endl;
    return false;
  }

  // Extract the first two bytes
  uint8_t byte1 = buffer[0];
  uint8_t byte2 = buffer[1];

  // Check if this is a text frame (opcode 0x1)
  uint8_t opcode = byte1 & 0x0F;
  if (opcode != 0x1) {
    std::cerr << "[decode_frame] Unsupported opcode: " << static_cast<int>(opcode) << std::endl;
    return false;
  }

  // Determine payload length
  size_t payload_len = byte2 & 0x7F;
  size_t offset = 2;

  if (payload_len == 126) {
    if (len < 4) {
      std::cerr << "[decode_frame] Frame too short for extended payload length (126)" << std::endl;
      return false;
    }
    payload_len = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]);
    offset += 2;
  } else if (payload_len == 127) {
    if (len < 10) {
      std::cerr << "[decode_frame] Frame too short for extended payload length (127)" << std::endl;
      return false;
    }
    payload_len = 0;
    for (int i = 0; i < 8; ++i) {
      payload_len = (payload_len << 8) | static_cast<uint8_t>(buffer[2 + i]);
    }
    offset += 8;
  }

  // Check if the frame is masked
  bool is_masked = byte2 & 0x80;
  if (!is_masked) {
    std::cerr << "[decode_frame] Frame is not masked (client frames must be masked)" << std::endl;
    return false;
  }

  // Extract masking key
  if (len < offset + 4) {
    std::cerr << "[decode_frame] Frame too short for masking key" << std::endl;
    return false;
  }
  uint8_t mask[4];
  memcpy(mask, buffer + offset, 4);
  offset += 4;

  // Extract and unmask the payload
  if (len < offset + payload_len) {
    std::cerr << "[decode_frame] Frame too short for payload" << std::endl;
    return false;
  }
  message.resize(payload_len);
  for (size_t i = 0; i < payload_len; ++i) {
    message[i] = buffer[offset + i] ^ mask[i % 4];
  }

  return true;
}

bool WebSocketServer::perform_handshake(int client_socket) {
  char buffer[4096];
  int len = 0;

  // Retry reading the handshake request
  while (true) {
    len = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (len < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // No data available, wait and retry
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      } else {
        perror("[perform_handshake] recv failed");
        return false;
      }
    } else if (len == 0) {
      std::cerr << _deb("Client closed the connection", __func__) << std::endl;
      return false;
    }
    break; // Data received successfully
  }

  buffer[len] = '\0';
  std::string request(buffer);
  std::cout << _deb("Received handshake request:\n" + request, __func__) << std::endl;

  // Extract Sec-WebSocket-Key
  size_t key_pos = request.find("Sec-WebSocket-Key: ");
  if (key_pos == std::string::npos) {
    std::cerr << _deb("Missing Sec-WebSocket-Key in handshake request", __func__) << std::endl;
    return false;
  }
  size_t key_end = request.find("\r\n", key_pos);
  std::string key = request.substr(key_pos + 19, key_end - key_pos - 19);

  // Generate Sec-WebSocket-Accept
  key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(key.c_str()), key.size(), hash);

  BIO* bio = BIO_new(BIO_s_mem());
  BIO* b64 = BIO_new(BIO_f_base64());
  bio = BIO_push(b64, bio);
  BIO_write(bio, hash, SHA_DIGEST_LENGTH);
  BIO_flush(bio);

  BUF_MEM* buffer_ptr;
  BIO_get_mem_ptr(bio, &buffer_ptr);
  std::string accept_key(buffer_ptr->data, buffer_ptr->length - 1);
  BIO_free_all(bio);

  std::cout << _deb("Generated Sec-WebSocket-Accept: " + accept_key, __func__) << std::endl;

  // Send handshake response
  std::ostringstream response;
  response << "HTTP/1.1 101 Switching Protocols\r\n";
  response << "Upgrade: websocket\r\n";
  response << "Connection: Upgrade\r\n";
  response << "Sec-WebSocket-Accept: " << accept_key << "\r\n";
  response << "\r\n";

  std::string response_str = response.str();
  if (send(client_socket, response_str.c_str(), response_str.size(), 0) <= 0) {
    perror("[perform_handshake] Failed to send handshake response");
    return false;
  }

  std::cout << _deb("Handshake successful", __func__) << std::endl;
  return true;
}