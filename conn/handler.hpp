#pragma once

#include "../websocket/client.hpp"
#include "../util/thread_queue.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

class ApiHandler {
public:
  ApiHandler(std::vector<int> ports, WebSocketClient& ws);
  ~ApiHandler();

  void run();

private:
  std::vector<int> ports;
  WebSocketClient& ws;

  void handle_client(int client_socket);
  void start_port(int port);

  void forward_to_ws(const std::string& request, int client_socket);
  void send_response_to_user(const std::string& response, int client_socket);
};