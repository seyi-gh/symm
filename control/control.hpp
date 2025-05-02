#include "../conn/handler.hpp"
#include "../websocket/server.hpp"

#include <string>
#include <vector>

class Control {
public:
  Control(std::vector<short int>& listen_ports, short int& ws_port) {}
  ~Control() {}

private:
  short int ws_port;
  std::vector<short int> listen_ports;
};