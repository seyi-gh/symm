#pragma once

#include "../util/thread_queue.h"

#include <string>
#include <vector>
#include <cstdint>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <netinet/in.h>

class wsutils {
public:
  static std::string generate_sec_ws_key();
  static std::ostringstream generate_headers(const std::vector<std::string> headers);
};

class WebSocketClient {
public:
  WebSocketClient(const std::string& host, int port);
  ~WebSocketClient();

  bool connect();
  bool send(const std::vector<uint8_t>& data);
  bool receive(std::vector<uint8_t>& data);
  bool is_connected() const;

private:
  std::string host_;
  int port_;
  int socket_fd_;
  bool connected_;

  bool perform_handshake();
};