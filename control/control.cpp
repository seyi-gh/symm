#include "control.hpp"

Control::Control(std::vector<short int>& listen_port, short int& ws_port) {
  listen_port = listen_ports;
  ws_port = ws_port;

  WebSocketServer ws_server(ws_port);
  ApiHandler api_handler(listen_ports);
}
Control::~Control() {}

