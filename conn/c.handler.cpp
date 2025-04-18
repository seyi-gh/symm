#include "c.handler.hpp"
#include "../util/pck.h"

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>


ApiHandler::ApiHandler(std::vector<int> ports)
  :ports(ports) {}
ApiHandler::~ApiHandler() {}


void ApiHandler::handle_client(int client_socket) {
  char buffer[1024] = {0};
  int valread = read(client_socket, buffer, sizeof(buffer));
  if (valread > 0) {
    std::string request = std::string(buffer, valread);
    
    // Send the response
    pck response = pck(200);
    //response.add_con_cls();
    response.set_content("Hello, World!");
    std::string e_response = response.export_();

    send(client_socket, e_response.c_str(), e_response.length(), 0);
    std::cout << "Response sent to client: " << inet_ntoa(((struct sockaddr_in*)&client_socket)->sin_addr) << ":" << ntohs(((struct sockaddr_in*)&client_socket)->sin_port) << std::endl;
  }
  close(client_socket);
}

void ApiHandler::start_port(int port) {
  int server_fd, client_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);

  // Create socket file descriptor
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }
    
  // Set socket options
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
    
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
    
  // Bind socket to port
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
    
  // Start listening
  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }
    
  std::cout << "Server listening on port " << port << std::endl;
    
  while (true) {
    if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
      perror("accept");
      continue;
    }

    // Handle client in a separate thread
    std::thread client_thread(&ApiHandler::handle_client, this, client_socket);
    client_thread.detach(); // Detach the thread to allow it to run independently
    std::cout << "Accepted connection from " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;
  }
}

void ApiHandler::run() {
  std::vector<std::thread> threads;
  for (int port : ports) {
    threads.emplace_back(&ApiHandler::start_port, this, port);
  }
  for (auto& thread : threads) {
    thread.join();
  }
}