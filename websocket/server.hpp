#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <sys/epoll.h>
#include "../util/logger.h"

class WebSocketServer {
public:
  WebSocketServer(int port);
  ~WebSocketServer();

  void run();
  void stop();

private:
  int port_;
  int server_fd_;
  int epoll_fd_;
  // Partial buffer for client handler
  std::unordered_map<int, std::string> client_buffers_;
  // Epoll event structure
  std::mutex client_mutex_;
  bool running_;

  bool perform_handshake(int client_socket);
  bool decode_frame(const char* buffer, long len, std::string& message);
  void handle_client_read(int client_socket);
  void handle_client_write(int client_socket, const std::string& message);
  void broadcast_message(const std::vector<uint8_t>& frame, const std::vector<int>& client_sockets);
  bool setup_server_socket();
  void handle_events();
};