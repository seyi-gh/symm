#include "websocket/server.hpp"
#include "conn/handler.hpp"
#include <iostream>


int main() {
  int port = 9000;
  WebSocketServer server(port);

  //ApiHandler apiHandler({3000, 5000});

  //apiHandler.run();
  server.run();

  return 0;
}