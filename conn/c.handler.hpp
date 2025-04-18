#pragma once

#include <string>
#include <vector>

class ApiHandler {
private:
public:
  std::vector<int> ports;

  ApiHandler(std::vector<int> ports);
  ~ApiHandler();

  void handle_client(int client_socket);
  void start_port(int port);

  void run();
};