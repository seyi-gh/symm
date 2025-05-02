#pragma once

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>

class ApiHandler {
public:
  explicit ApiHandler(const std::vector<short int>& ports);
  ~ApiHandler();

  void run(); // Start listening on all ports
  virtual void process_request(int client_socket); // Process client request and send response

private:
  std::vector<short int> ports_; // List of ports to listen on
  std::vector<std::thread> threads_; // Threads for each port
  std::unordered_map<int, std::vector<pollfd>> poll_fds_; // Poll file descriptors for each port
  std::mutex poll_mutex_; // Mutex for thread-safe access to poll_fds_

  void start_port(int port); // Start listening on a specific port
};