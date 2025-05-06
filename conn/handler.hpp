#pragma once

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>

class ApiHandler {
public:
  struct self_port {
    int port, sfd, opt;
    struct sockaddr_in addr;
    self_port(int p, int sfd, struct sockaddr_in addr, int o);
  };

  using RequestHandler = std::function<std::string(const std::string&)>;

  ApiHandler(const std::vector<int>& ports);
  virtual ~ApiHandler();

  void run(); // Start listening on all ports
  void set_request_handler(RequestHandler handler); // Set custom request handler

protected:
  virtual std::string process_http_request(const std::string& request); // Process HTTP requests
  virtual void process_client_data(int client_socket, const std::string& data); // Process client data

private:
  std::vector<self_port> id_ports; // List of ports to listen on
  std::vector<std::thread> threads_; // Threads for each port
  std::mutex poll_mutex_; // Mutex for thread-safe access to poll_fds_

  self_port setup_port(int port); // Start listening on a specific port
  void listen_port(self_port& port_id); // Listen for incoming connections on a port
  void process_request(int client_socket); // Process client request and send response

  RequestHandler custom_request_handler_; // Custom handler for HTTP requests
};