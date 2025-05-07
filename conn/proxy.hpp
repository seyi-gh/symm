#pragma once

#include "../util/pck.h"

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <atomic>

class ApiProxy {
public:
  using DataHandler = std::function<http_pck(const std::string&, int)>;

  explicit ApiProxy(const std::vector<int>& ports);
  ~ApiProxy();

  void run();
  void set_data_handler(DataHandler handler);

  std::string response_get_host(std::string request);
  std::string response_get_path(std::string request);

protected:
  struct PortInfo {
    int port;
    int sfd;
    struct sockaddr_in addr;
    PortInfo(int p, int fd, struct sockaddr_in a) : port(p), sfd(fd), addr(a) {}
  };

  virtual http_pck process_data(const std::string& request, int client_fd);

private:
  std::vector<PortInfo> ports_;
  std::vector<std::thread> threads_;
  std::mutex ports_mutex_;
  std::atomic<bool> running_{true};
  DataHandler custom_handler_;

  PortInfo setup_port(int port);
  void listen_on_port(PortInfo port_info);
  void handle_client(int client_fd, int listen_port);
};