#include "websocket/ws.hpp"
#include "conn/proxy.hpp"
#include "util/logger.h"
#include <iostream>
#include <thread>

int main() {
  /*
  ApiProxy proxy({3000, 5000});
  proxy.set_data_handler([&](const std::string& request, int client_fd) -> http_pck {
    //logger::info(proxy.response_get_host(request), __func__);
    //logger::info(proxy.response_get_path(request), __func__);
    logger::info("Client socket: " + std::to_string(client_fd), "lambda");

    http_pck response;
    response.set_status(200);
    response.set_content("Content-Type", "text/plain");
    response.set_body("Hello response from server!");
    // Set other fields of http_pck as needed
    return response;
  });
  proxy.run();
  */

  ApiProxy proxy({3000, 5000});
  WebSocketServer ws_server(9000, 4);

  proxy.set_data_handler([&](const std::string& request, int client_fd) -> http_pck {
    // Broadcast the request to all connected WebSocket clients
    logger::info("Received request: " + request, "lambda");
    ws_server.broadcast(request);
    logger::info("Client socket: " + std::to_string(client_fd), "lambda");

    // Wait for a response from any WebSocket client (simple synchronous example)
    std::string ws_response = ws_server.receive_response(); // You need to implement receive_response() in WebSocketServer
    if (ws_response.empty()) {
      logger::warn("No response from WebSocket client", "lambda");
    } else {
      logger::info("Received response from WebSocket client: " + ws_response, "lambda");
    }


    http_pck response;
    response.set_status(200);
    response.set_content("Content-Type", "text/plain");
    response.set_body(ws_response.empty() ? "No response from WebSocket client" : ws_response);
    logger::info("Response body: " + response.content_data, "lambda");
    return response;
  });

  std::thread ws_thread([&]() { ws_server.run(); });
  proxy.run();
  ws_thread.join();
  return 0;
}