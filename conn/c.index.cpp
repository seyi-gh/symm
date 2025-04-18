#include "c.handler.cpp"

int main() {
  std::vector<int> ports = { 3000 };
  std::vector<std::thread> server_threads;
  
  ApiHandler api_handler(ports); //? Initialize the API handler with the ports

  api_handler.run(); //? Start the API handler
  
  return 0;
}