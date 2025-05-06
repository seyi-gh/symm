#include "handler.hpp"
#include "../util/logger.h"
#include "../util/pck.h"

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

ApiHandler::ApiHandler(const std::vector<int>& ports) {
  for (int port : ports)
    id_ports.push_back(setup_port(port));
}

ApiHandler::~ApiHandler() {}

ApiHandler::self_port::self_port(int p, int fd, struct sockaddr_in addr, int o)
  : port(p), sfd(fd), addr(addr), opt(o) {}

ApiHandler::self_port ApiHandler::setup_port(int port) {
  self_port port_id = self_port(port, -1, {}, -1);

  // Create socket file descriptor
  if ((port_id.sfd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    logger::error("Socket creation failed for port " + std::to_string(port), __func__);
  }

  // Set socket options
  if (setsockopt(port_id.sfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &port_id.opt, sizeof(port_id.opt))) {
    logger::error("Failed to set socket options for port " + std::to_string(port), __func__);
    close(port_id.sfd);
  }

  // Set socket to non-blocking mode
  if (fcntl(port_id.sfd, F_SETFL, O_NONBLOCK) < 0) {
    logger::error("Failed to set non-blocking mode for port " + std::to_string(port), __func__);
    close(port_id.sfd);
  }

  port_id.addr.sin_family = AF_INET;
  port_id.addr.sin_addr.s_addr = INADDR_ANY;
  port_id.addr.sin_port = htons(port);

  // Bind socket to port
  if (bind(port_id.sfd, (struct sockaddr*)&port_id.addr, sizeof(port_id.addr)) < 0) {
    logger::error("Failed to bind socket to port " + std::to_string(port), __func__);
    close(port_id.sfd);
  }

  return port_id;
}

void ApiHandler::listen_port(self_port& port_id) {
  if (listen(port_id.sfd, SOMAXCONN) < 0) {
    logger::error("Failed to listen on port " + std::to_string(port_id.port), __func__);
    close(port_id.sfd);
    return;
  }

  std::vector<pollfd> fds;
  pollfd server_poll_fd = {port_id.sfd, POLLIN, 0};
  fds.push_back(server_poll_fd);

  while (true) {
    int poll_count = poll(fds.data(), fds.size(), -1);
    if (poll_count < 0) {
      logger::error("Poll failed on port " + std::to_string(port_id.port), __func__);
      break;
    }

    for (size_t i = 0; i < fds.size(); ++i) {
      if (fds[i].revents & POLLIN) {
        if (fds[i].fd == port_id.sfd) {
          struct sockaddr_in client_address;
          socklen_t client_len = sizeof(client_address);
          int client_socket = accept(port_id.sfd, (struct sockaddr*)&client_address, &client_len);
          if (client_socket < 0) {
            logger::error("Failed to accept client connection on port " + std::to_string(port_id.port), __func__);
            continue;
          }

          if (fcntl(client_socket, F_SETFL, O_NONBLOCK) < 0) {
            logger::error("Failed to set non-blocking mode for client socket", __func__);
            close(client_socket);
            continue;
          }

          pollfd client_poll_fd = {client_socket, POLLIN, 0};
          fds.push_back(client_poll_fd);
        } else {
          std::thread(&ApiHandler::process_request, this, fds[i].fd).detach();
          fds.erase(fds.begin() + i);
          --i;
        }
      }
    }
  }
  close(port_id.sfd);
}

void ApiHandler::process_request(int client_socket) {
  char buffer[1024];
  memset(buffer, 0, sizeof(buffer));

  ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
  if (bytes_read <= 0) {
    logger::error("Failed to read from client socket", __func__);
    close(client_socket);
    return;
  }

  std::string request(buffer);
  logger::info("Received request:\n" + request, __func__);

  std::string response;
  if (custom_request_handler_) {
    response = custom_request_handler_(request);
  } else {
    response = process_http_request(request);
  }

  if (send(client_socket, response.c_str(), response.size(), 0) < 0) {
    logger::error("Failed to send response to client", __func__);
  }

  close(client_socket);
}

std::string ApiHandler::process_http_request(const std::string& request) {
  http_pck pck_response(200);
  pck_response.set_content("Content-Type", "text/plain");
  pck_response.set_body("Hello World!");
  return pck_response.export_packet();
}

void ApiHandler::set_request_handler(RequestHandler handler) {
  custom_request_handler_ = handler;
}

void ApiHandler::run() {
  for (self_port& id_port : id_ports)
    threads_.emplace_back(&ApiHandler::listen_port, this, std::ref(id_port));
  for (auto& thread : threads_)
    if (thread.joinable()) thread.join();
}

void ApiHandler::process_client_data(int client_socket, const std::string& data) {
  // Custom processing of client data can be implemented here
  logger::info("Processing client data: " + data, __func__);
}