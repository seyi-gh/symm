#include "conn/c.handler.hpp"
#include "websocket/ws.client.hpp"

#include <iostream>
#include <thread>
#include <chrono>

int main() {
  WebSocketClient ws_client("localhost", 5000); // Connect to WebSocket server on port 5000
  std::vector<int> ports = {3000}; // Ports for the API handler

  ApiHandler api_handler(ports, ws_client);
  api_handler.run();

  return 0;
}



/*
int counter = 1;
  while (true) {
    std::string message = "Hello World!" + std::to_string(counter++);
    std::vector<uint8_t> data(message.begin(), message.end());

    if (!ws_client.send(data, false)) {
      std::cerr << "Failed to send message" << std::endl;
      break;
    }

    std::cout << "Sent: " << message << std::endl;

    // Wait for 2 seconds before sending the next message
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  }
*/