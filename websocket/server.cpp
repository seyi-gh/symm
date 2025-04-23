#include "../util/pck.h"
#include "../util/logger.h"
#include "server.hpp"

#include <chrono>
#include <vector>
#include <string>
#include <cerrno>
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
#include <openssl/evp.h>
#include <thread>
#include <algorithm>

// WebSocket frame constants
constexpr uint8_t WS_FIN_TEXT_FRAME = 0x81; // FIN + text frame (opcode 0x1)
constexpr uint8_t WS_PAYLOAD_LEN_16 = 126;  // Extended payload length (16-bit)
constexpr uint8_t WS_PAYLOAD_LEN_64 = 127;  // Extended payload length (64-bit)

#define MAX_EVENTS 10

WebSocketServer::WebSocketServer(int port) : port_(port), server_fd_(-1), epoll_fd_(-1), running_(true) {}

WebSocketServer::~WebSocketServer() {
    if (server_fd_ != -1) close(server_fd_);
    if (epoll_fd_ != -1) close(epoll_fd_);
}

void WebSocketServer::stop() {
    running_ = false;
}

bool WebSocketServer::setup_server_socket() {
    struct sockaddr_in address;
    int opt = 1;

    // Create server socket
    if ((server_fd_ = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cout << log.error("Socket creation failed", __func__) << std::endl;
        return false;
    }

    // Set socket options
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        std::cout << log.error("Failed to set SO_REUSEADDR option", __func__) << std::endl;
        return false;
    }

    if (fcntl(server_fd_, F_SETFL, O_NONBLOCK) < 0) {
        std::cout << log.error("Failed to set non-blocking mode", __func__) << std::endl;
        return false;
    }

    // Bind socket to port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cout << log.error("Failed to bind socket to port", __func__) << std::endl;
        return false;
    }

    // Start listening
    if (listen(server_fd_, SOMAXCONN) < 0) {
        std::cout << log.error("Failed to listen on socket", __func__) << std::endl;
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
        std::cout << log.error("Failed to create epoll instance", __func__) << std::endl;
        exit(EXIT_FAILURE);
    }

    // Add server socket to epoll
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &event) < 0) {
        std::cout << log.error("Failed to add server socket to epoll", __func__) << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << log.info("WebSocket server running on port " + std::to_string(port_), __func__) << std::endl;

    // Event loop
    handle_events();
}

void WebSocketServer::handle_events() {
    while (running_) {
        struct epoll_event events[MAX_EVENTS];

        int num_events = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
        if (num_events < 0) {
            std::cout << log.error("epoll_wait failed", __func__) << std::endl;
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
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    } else {
                        std::cout << log.error("Failed to accept client connection", __func__) << std::endl;
                        continue;
                    }
                }

                // Set client socket to non-blocking mode
                if (fcntl(client_socket, F_SETFL, O_NONBLOCK) < 0) {
                    std::cout << log.error("Failed to set client socket to non-blocking mode", __func__) << std::endl;
                    close(client_socket);
                    continue;
                }

                // Perform WebSocket handshake
                if (!perform_handshake(client_socket)) {
                    close(client_socket);
                    continue;
                }

                // Add client socket to epoll
                struct epoll_event client_event;
                client_event.events = EPOLLIN | EPOLLET; // Edge-triggered
                client_event.data.fd = client_socket;
                if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_socket, &client_event) < 0) {
                    close(client_socket);
                    continue;
                }

                std::cout << log.info("New client connected: " + std::to_string(client_socket), __func__) << std::endl;
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
        if (len < 0) {
            std::cout << log.error("Error reading from client socket", __func__) << std::endl;
        } else {
            std::cout << log.info("Client disconnected: " + std::to_string(client_socket), __func__) << std::endl;
        }
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_socket, nullptr);
        close(client_socket);
        return;
    }

    // Process the message
    std::string message;
    if (!decode_frame(buffer, len, message)) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_socket, nullptr);
        close(client_socket);
        return;
    }

    std::cout << log.info("Received message from client: " + message, __func__) << std::endl;

    // Prepare WebSocket frame
    std::vector<uint8_t> frame;
    frame.push_back(WS_FIN_TEXT_FRAME); // FIN + text frame (opcode 0x1)

    size_t payload_len = message.size();
    if (payload_len <= 125) {
        frame.push_back(static_cast<uint8_t>(payload_len)); // No mask bit for server-to-client frames
    } else if (payload_len <= 65535) {
        frame.push_back(WS_PAYLOAD_LEN_16); // Extended payload length (16-bit)
        frame.push_back((payload_len >> 8) & 0xFF);
        frame.push_back(payload_len & 0xFF);
    } else {
        frame.push_back(WS_PAYLOAD_LEN_64); // Extended payload length (64-bit)
        for (int i = 7; i >= 0; --i) {
            frame.push_back((payload_len >> (i * 8)) & 0xFF);
        }
    }

    // Append the payload
    frame.insert(frame.end(), message.begin(), message.end());

    // Send the frame
    size_t total_sent = 0;
    while (total_sent < frame.size()) {
        ssize_t sent = send(client_socket, frame.data() + total_sent, frame.size() - total_sent, 0);
        if (sent < 0) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_socket, nullptr);
            close(client_socket);
            return;
        }
        total_sent += sent;
    }
}

bool WebSocketServer::perform_handshake(int client_socket) {
    char buffer[4096];
    int len = 0;

    // Retry reading the handshake request
    for (int retries = 0; retries < 10; ++retries) { // Retry up to 10 times
        len = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cerr << log.error("No data available to read (non-blocking mode), retrying...", __func__) << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait 100ms before retrying
                continue;
            } else {
                perror("[perform_handshake] recv failed");
                return false;
            }
        } else if (len == 0) {
            std::cerr << log.error("Client closed the connection", __func__) << std::endl;
            return false;
        }
        break; // Data received successfully
    }

    if (len <= 0) {
        std::cerr << log.error("Failed to receive handshake request after retries", __func__) << std::endl;
        return false;
    }

    buffer[len] = '\0';
    std::string request(buffer);
    std::cout << log.debug("Received handshake request:\n" + request, __func__) << std::endl;

    // Extract Sec-WebSocket-Key
    size_t key_pos = request.find("Sec-WebSocket-Key: ");
    if (key_pos == std::string::npos) {
        std::cerr << log.error("Missing Sec-WebSocket-Key in handshake request", __func__) << std::endl;
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

    std::cout << log.debug("Generated Sec-WebSocket-Accept: " + accept_key, __func__) << std::endl;

    // Send handshake response
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << accept_key << "\r\n";
    response << "\r\n";

    if (send(client_socket, response.str().c_str(), response.str().size(), 0) <= 0) {
        perror("[perform_handshake] Failed to send handshake response");
        return false;
    }

    std::cout << log.info("Handshake successful", __func__) << std::endl;
    return true;
}

bool WebSocketServer::decode_frame(const char* buffer, long len, std::string& message) {
  if (buffer == nullptr) {
    std::cerr << log.error("Null buffer pointer passed to decode_frame", __func__) << std::endl;
    return false;
  }
  
  if (len < 2) {
    std::cerr << log.error("Frame too short to decode", __func__) << std::endl;
    return false;
  }

  // Extract the first two bytes
  uint8_t byte1 = buffer[0];
  uint8_t byte2 = buffer[1];

  // Check if this is a text frame (opcode 0x1)
  uint8_t opcode = byte1 & 0x0F;
  if (opcode != 0x1) {
    std::cerr << log.error("Unsupported opcode: " + std::to_string(opcode), __func__) << std::endl;
    return false;
  }

  // Determine payload length
  size_t payload_len = byte2 & 0x7F;
  size_t offset = 2;

  if (payload_len == 126) {
    if (len < 4) {
      std::cerr << log.error("Frame too short for extended payload length (126)", __func__) << std::endl;
      return false;
    }
    payload_len = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]);
    offset += 2;
  } else if (payload_len == 127) {
    if (len < 10) {
      std::cerr << log.error("Frame too short for extended payload length (127)", __func__) << std::endl;
      return false;
    }
    if (len < offset + 8) { // Ensure there are enough bytes for the extended payload length
      std::cerr << log.error("Insufficient data for extended payload length (127)", __func__) << std::endl;
      return false;
    }
    payload_len = 0;

    if (payload_len > static_cast<size_t>(len) - offset - 4) {
      std::cerr << log.error("Payload length exceeds available data", __func__) << std::endl;
      return false;
    }

    for (int i = 0; i < 8; ++i) {
      payload_len = (payload_len << 8) | static_cast<uint8_t>(buffer[2 + i]);
    }
    offset += 8;
  }

  // Check if the frame contains enough data for the payload and masking key
  if (len < offset + 4 + payload_len) {
    std::cerr << log.error("Frame too short for payload and masking key", __func__) << std::endl;
    return false;
  }

  // Extract the masking key
  uint8_t mask[4];
  mask[0] = buffer[offset];
  mask[1] = buffer[offset + 1];
  mask[2] = buffer[offset + 2];
  mask[3] = buffer[offset + 3];

  offset += 4;

  // Unmask the payload
  message.resize(payload_len);
  std::transform(buffer + offset, buffer + offset + payload_len, message.begin(),
    [mask](char byte, size_t index = 0) mutable {
    return byte ^ mask[index++ % 4];
  });

  return true;
}