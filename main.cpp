#include "websocket/server.hpp"
#include <iostream>

int main() {
  int port = 9000;
  WebSocketServer server(port);
  
  server.run();

  return 0;
}