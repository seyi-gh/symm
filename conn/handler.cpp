#include "handler.hpp"
#include "../util/pck.h"

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

ApiHandler::ApiHandler(std::vector<int> ports, WebSocketClient& ws)
  : ports(ports), ws(ws) {
  if (!ws.connect()) {
    std::cerr << "Failed to connect to WebSocket server" << std::endl;
    exit(EXIT_FAILURE);
  }
}

ApiHandler::~ApiHandler() {}

void ApiHandler::handle_client(int client_socket) {
  std::cout << "Client connected: " << client_socket << std::endl;

  char buffer[1024] = {0};
  int valread = read(client_socket, buffer, sizeof(buffer));
  if (valread > 0) {
    std::string request(buffer, valread);
    std::cout << "[ApiHandler] Received request: " << request << std::endl;

    // Forward the request to the WebSocket server
    forward_to_ws(request, client_socket);
  } else {
    std::cerr << "[ApiHandler] Failed to read from client" << std::endl;
  }

  close(client_socket);
}

void ApiHandler::forward_to_ws(const std::string& request, int client_socket) {
  // Send the request to the WebSocket server
  if (!ws.send(std::vector<uint8_t>(request.begin(), request.end()))) {
    std::cerr << "[ApiHandler] Failed to forward request to WebSocket server" << std::endl;
    return;
  }

  std::cout << "[ApiHandler] Forwarded request to WebSocket server: " << request << std::endl;

  // Wait for the response from the WebSocket server
  std::vector<uint8_t> response_data;
  if (ws.receive(response_data)) {
    std::cout << "[ApiHandler] Received raw response data from WebSocket server" << std::endl;

    // Convert response data to string
    std::string ws_response(response_data.begin(), response_data.end());
    std::cout << "[ApiHandler] Received response from WebSocket server: " << ws_response << std::endl;

    // Validate response length
    if (ws_response.size() > 65535) {
      std::cerr << "[ApiHandler] Response too large, rejecting" << std::endl;
      send_response_to_user("HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n", client_socket);
      return;
    }

    //! TODO: Check if the response is valid JSON
    // Use pck to format the response as an HTTP/1.1 response
    std::cout << "[ApiHandler] Formatting response as HTTP/1.1" << std::endl;
    pck* response_packet = new pck(200);
    std::cout << "[ApiHandler] Setting content type to application/json" << std::endl;
    response_packet->set_content(ws_response);
    std::cout << "[ApiHandler] Adding headers" << std::endl;

    // Export the HTTP response
    std::string http_response;
    try {
      http_response = response_packet->export_();
      //! TODO: Invalid response
    } catch (const std::exception& e) {
      std::cerr << "[ApiHandler] Exception while exporting HTTP response: " << e.what() << std::endl;
      send_response_to_user("HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n", client_socket);
      return;
    }

    // Send the HTTP response back to the user
    send_response_to_user(http_response, client_socket);
  } else {
    std::cerr << "[ApiHandler] Failed to receive response from WebSocket server" << std::endl;

    // Send an HTTP 500 response to the user
    pck error_packet(500);
    error_packet.set_content("Internal Server Error");
    std::string error_response = error_packet.export_();
    send_response_to_user(error_response, client_socket);
  }
}

void ApiHandler::send_response_to_user(const std::string& response, int client_socket) {
  if (send(client_socket, response.c_str(), response.size(), 0) < 0) {
    perror("[ApiHandler] Failed to send response to user");
  } else {
    std::cout << "[ApiHandler] Sent response to user: " << response << std::endl;
  }
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
  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  // Start listening
  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  std::cout << "[ApiHandler] Listening on port " << port << std::endl;

  while (true) {
    client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    if (client_socket < 0) {
      perror("accept");
      continue;
    }

    // Handle client in a separate thread
    std::thread client_thread(&ApiHandler::handle_client, this, client_socket);
    client_thread.detach();
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