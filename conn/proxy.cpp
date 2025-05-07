#include "proxy.hpp"
#include "../util/logger.h"
#include "../util/pck.h"
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

ApiProxy::ApiProxy(const std::vector<int>& ports) {
  for (int port : ports) {
    ports_.push_back(setup_port(port));
  }
}

ApiProxy::~ApiProxy() {
  running_ = false;
  for (auto& t : threads_) {
    if (t.joinable()) t.join();
  }
  for (auto& p : ports_) {
    if (p.sfd >= 0) close(p.sfd);
  }
}

void ApiProxy::set_data_handler(DataHandler handler) {
  custom_handler_ = handler;
}

void ApiProxy::run() {
  for (const auto& port_info : ports_) {
    threads_.emplace_back(&ApiProxy::listen_on_port, this, port_info);
  }
  for (auto& t : threads_) {
    if (t.joinable()) t.join();
  }
}

ApiProxy::PortInfo ApiProxy::setup_port(int port) {
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd < 0) {
    logger::error("Socket creation failed for port " + std::to_string(port), __func__);
    return PortInfo(port, -1, {});
  }

  int opt = 1;
  setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
  fcntl(sfd, F_SETFL, O_NONBLOCK);

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    logger::error("Bind failed for port " + std::to_string(port), __func__);
    close(sfd);
    return PortInfo(port, -1, {});
  }

  return PortInfo(port, sfd, addr);
}

struct ClientState {
  std::string read_buffer;
  std::string write_buffer;
  bool closed = false;
};

void ApiProxy::listen_on_port(PortInfo port_info) {
  if (listen(port_info.sfd, SOMAXCONN) < 0) {
    logger::error("Listen failed on port " + std::to_string(port_info.port), __func__);
    close(port_info.sfd);
    return;
  }

  std::vector<pollfd> fds = { {port_info.sfd, POLLIN, 0} };
  std::unordered_map<int, ClientState> clients;

  while (running_) {
    int n = poll(fds.data(), fds.size(), 1000);
    if (n < 0) {
      logger::error("Poll failed on port " + std::to_string(port_info.port), __func__);
      break;
    }
    for (size_t i = 0; i < fds.size(); ++i) {
      if (fds[i].revents & POLLIN) {
        if (fds[i].fd == port_info.sfd) {
          struct sockaddr_in client_addr;
          socklen_t len = sizeof(client_addr);
          int client_fd = accept(port_info.sfd, (struct sockaddr*)&client_addr, &len);
          if (client_fd >= 0) {
            fcntl(client_fd, F_SETFL, O_NONBLOCK);
            fds.push_back({client_fd, POLLIN, 0});
            clients[client_fd] = ClientState();
          }
        } else {
          char buffer[4096];
          ssize_t n = recv(fds[i].fd, buffer, sizeof(buffer), 0);
          if (n <= 0) {
            close(fds[i].fd);
            clients.erase(fds[i].fd);
            fds.erase(fds.begin() + i);
            --i;
            continue;
          }
          clients[fds[i].fd].read_buffer.append(buffer, n);

          // Simple HTTP: check for end of headers
          if (clients[fds[i].fd].read_buffer.find("\r\n\r\n") != std::string::npos) {
            http_pck response;
            logger::info("Received request: " + clients[fds[i].fd].read_buffer, __func__);
            if (custom_handler_) {
              response = custom_handler_(clients[fds[i].fd].read_buffer, fds[i].fd);
            } else {
              response = process_data(clients[fds[i].fd].read_buffer, fds[i].fd);
            }
            clients[fds[i].fd].write_buffer = response.export_packet();
            fds[i].events = POLLOUT;
          }
        }
      } else if (fds[i].revents & POLLOUT) {
        auto& state = clients[fds[i].fd];
        ssize_t sent = send(fds[i].fd, state.write_buffer.c_str(), state.write_buffer.size(), 0);
        if (sent > 0) {
          state.write_buffer.erase(0, sent);
        }
        if (state.write_buffer.empty()) {
          close(fds[i].fd);
          clients.erase(fds[i].fd);
          fds.erase(fds.begin() + i);
          --i;
        }
      }
    }
  }
  close(port_info.sfd);
}

void ApiProxy::handle_client(int client_fd, int listen_port) {
  char buffer[4096];
  ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
  if (n <= 0) return;
  std::string request(buffer, n);
  http_pck response;

  if (custom_handler_) {
    response = custom_handler_(request, client_fd);
  } else {
    response = process_data(request, client_fd);
  }
  std::string s_response = response.export_packet();

  if (!s_response.empty()) {
    send(client_fd, s_response.c_str(), s_response.size(), 0);
  }
}

http_pck ApiProxy::process_data(const std::string& request, int client_fd) {
  http_pck response;
  response.set_status(200);
  response.set_content("Content-Type", "text/plain");
  response.set_body(request);
  return response;
}

std::string ApiProxy::response_get_host(std::string request) {
  std::string host = "";
  size_t pos = request.find("Host: ");
  if (pos != std::string::npos) {
    pos += 6; // Length of "Host: "
    size_t end_pos = request.find("\r\n", pos);
    if (end_pos != std::string::npos) {
      host = request.substr(pos, end_pos - pos);
    }
  }
  return host;
}

std::string ApiProxy::response_get_path(std::string request) {
  std::string path = "";
  size_t pos = request.find("GET ");
  if (pos != std::string::npos) {
    pos += 4; // Length of "GET "
    size_t end_pos = request.find(" ", pos);
    if (end_pos != std::string::npos) {
      path = request.substr(pos, end_pos - pos);
    }
  }
  return path;
}