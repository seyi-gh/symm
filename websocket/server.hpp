#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <sys/epoll.h>

class WebSocketServer {
public:
  WebSocketServer(int port);
  ~WebSocketServer();

  void run();

private:
  std::string _deb(const std::string& msg, const std::string& func) const;

  int port_;
  int server_fd_;
  int epoll_fd_;
  std::unordered_map<int, std::string> client_buffers_; // Buffers for partial reads
  std::mutex client_mutex_;

  bool setup_server_socket();
  void handle_events();
  void handle_client_read(int client_socket);
  void handle_client_write(int client_socket, const std::string& message);
  bool perform_handshake(int client_socket);
  bool decode_frame(const char* buffer, ssize_t len, std::string& message);
};