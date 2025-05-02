#include "handler.hpp"
#include "../util/logger.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

ApiHandler::ApiHandler(const std::vector<short int>& ports)
  : ports_(ports) {}

ApiHandler::~ApiHandler() {
  for (auto& thread : threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

void ApiHandler::run() {
  for (int port : ports_) {
    threads_.emplace_back(&ApiHandler::start_port, this, port);
  }

  // Wait for all threads to finish
  for (auto& thread : threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

void ApiHandler::start_port(int port) {
  int server_fd;
  struct sockaddr_in address;
  int opt = 1;

  // Create socket file descriptor
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    logger::error("Socket creation failed for port " + std::to_string(port), __func__);
    return;
  }

  // Set socket options
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    logger::error("Failed to set socket options for port " + std::to_string(port), __func__);
    close(server_fd);
    return;
  }

  // Set socket to non-blocking mode
  if (fcntl(server_fd, F_SETFL, O_NONBLOCK) < 0) {
    logger::error("Failed to set non-blocking mode for port " + std::to_string(port), __func__);
    close(server_fd);
    return;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  // Bind socket to port
  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    logger::error("Failed to bind socket to port " + std::to_string(port), __func__);
    close(server_fd);
    return;
  }

  // Start listening
  if (listen(server_fd, SOMAXCONN) < 0) {
    logger::error("Failed to listen on port " + std::to_string(port), __func__);
    close(server_fd);
    return;
  }

  logger::info("Listening on port " + std::to_string(port), __func__);

  // Polling setup
  std::vector<pollfd> fds;
  pollfd server_poll_fd = {server_fd, POLLIN, 0};
  fds.push_back(server_poll_fd);

  while (true) {
    int poll_count = poll(fds.data(), fds.size(), -1);
    if (poll_count < 0) {
      logger::error("Poll failed on port " + std::to_string(port), __func__);
      break;
    }

    for (size_t i = 0; i < fds.size(); ++i) {
      if (fds[i].revents & POLLIN) {
        if (fds[i].fd == server_fd) {
          // Accept new client connection
          struct sockaddr_in client_address;
          socklen_t client_len = sizeof(client_address);
          int client_socket = accept(server_fd, (struct sockaddr*)&client_address, &client_len);
          if (client_socket < 0) {
            logger::error("Failed to accept client connection on port " + std::to_string(port), __func__);
            continue;
          }

          // Set client socket to non-blocking mode
          if (fcntl(client_socket, F_SETFL, O_NONBLOCK) < 0) {
            logger::error("Failed to set non-blocking mode for client socket", __func__);
            close(client_socket);
            continue;
          }

          logger::info("Accepted client connection on port " + std::to_string(port), __func__);

          // Add client socket to poll list
          pollfd client_poll_fd = {client_socket, POLLIN, 0};
          fds.push_back(client_poll_fd);
        } else {
          // Handle client request
          process_request(fds[i].fd);

          // Remove client socket from poll list
          close(fds[i].fd);
          fds.erase(fds.begin() + i);
          --i;
        }
      }
    }
  }

  close(server_fd);
}


void ApiHandler::process_request(int client_socket) {
  char buffer[1024];
  memset(buffer, 0, sizeof(buffer));

  ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
  if (bytes_read <= 0) {
    logger::error("Failed to read from client socket", __func__);
    return;
  }

  logger::info("Received request:\n" + std::string(buffer), __func__);

  // Send a basic HTTP response
  std::string response = "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 12\r\n"
    "\r\n"
    "Hello World!";
  if (send(client_socket, response.c_str(), response.size(), 0) < 0) {
    logger::error("Failed to send response to client", __func__);
  } else {
    logger::info("Sent response to client", __func__);
  }
}
