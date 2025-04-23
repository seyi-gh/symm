#include "websocket/server.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <cassert>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

void test_websocket_server() {
  std::cout << "[Test] Starting WebSocket server test..." << std::endl;

  // Start the WebSocket server in a separate thread
  WebSocketServer server(9000);
  std::thread server_thread([&server]() {
    server.run();
  });

  // Give the server some time to start
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Simulate a WebSocket client
  int client_socket = socket(AF_INET, SOCK_STREAM, 0);
  assert(client_socket >= 0 && "[Test] Failed to create client socket");

  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(9000);
  inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

  int connect_status = connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address));
  assert(connect_status == 0 && "[Test] Failed to connect to WebSocket server");

  std::cout << "[Test] Connected to WebSocket server" << std::endl;

  // Send a WebSocket handshake request
  std::string handshake_request =
      "GET / HTTP/1.1\r\n"
      "Host: localhost:9000\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Version: 13\r\n\r\n";

  if (send(client_socket, handshake_request.c_str(), handshake_request.size(), 0) < 0) {
    perror("[Test] Failed to send handshake request");
    close(client_socket);
    server_thread.join();
    return;
  }

  // Receive the handshake response
  char buffer[1024] = {0};
  ssize_t recv_len = recv(client_socket, buffer, sizeof(buffer), 0);
  if (recv_len <= 0) {
    perror("[Test] Failed to receive handshake response");
    close(client_socket);
    server_thread.join();
    return;
  }
  buffer[recv_len] = '\0'; // Null-terminate the response
  std::cout << "[Test] Handshake response passed" << std::endl;

  // Validate handshake response
  if (std::string(buffer).find("HTTP/1.1 101 Switching Protocols") == std::string::npos) {
    std::cerr << "[Test] Handshake failed - invalid response" << std::endl;
    close(client_socket);
    server_thread.join();
    return;
  }

  // Send a WebSocket frame (text message)
  std::string message = "Hello, WebSocket!";
  std::vector<uint8_t> frame;

  // FIN + text frame (opcode 0x1)
  frame.push_back(0x81);

  // Mask bit set + payload length
  size_t payload_len = message.size();
  if (payload_len <= 125) {
    frame.push_back(0x80 | payload_len); // Mask bit set
  } else if (payload_len <= 65535) {
    frame.push_back(0x80 | 126); // Mask bit set + extended payload length (16-bit)
    frame.push_back((payload_len >> 8) & 0xFF);
    frame.push_back(payload_len & 0xFF);
  } else {
    frame.push_back(0x80 | 127); // Mask bit set + extended payload length (64-bit)
    for (int i = 7; i >= 0; --i) {
      frame.push_back((payload_len >> (i * 8)) & 0xFF);
    }
  }

  // Generate a random masking key
  uint8_t mask[4];
  for (int i = 0; i < 4; ++i) {
    mask[i] = rand() % 256;
  }
  frame.insert(frame.end(), mask, mask + 4);

  // Mask the payload
  for (size_t i = 0; i < payload_len; ++i) {
    frame.push_back(message[i] ^ mask[i % 4]);
  }

  // Send the frame
  if (send(client_socket, frame.data(), frame.size(), 0) < 0) {
    perror("[Test] Failed to send WebSocket frame");
    close(client_socket);
    server_thread.join();
    return;
  }

  // Receive the echoed message
  char response[1024] = {0};
  recv_len = recv(client_socket, response, sizeof(response), 0); // Reuse recv_len
  if (recv_len <= 0) {
    perror("[Test] Failed to receive echoed message");
    close(client_socket);
    server_thread.join();
    return;
  }

  // Parse the WebSocket frame
  if (recv_len < 2) {
    std::cerr << "[Test] Frame too short" << std::endl;
    close(client_socket);
    server_thread.join();
    return;
  }

  // Extract the first two bytes
  uint8_t byte1 = response[0];
  uint8_t byte2 = response[1];

  // Check if this is a text frame (opcode 0x1)
  uint8_t opcode = byte1 & 0x0F;
  if (opcode != 0x1) {
    std::cerr << "[Test] Unsupported opcode: " << static_cast<int>(opcode) << std::endl;
    close(client_socket);
    server_thread.join();
    return;
  }

  // Determine payload length
  payload_len = byte2 & 0x7F; // Reuse payload_len
  size_t offset = 2;

  if (payload_len == 126) {
    if (recv_len < 4) {
      std::cerr << "[Test] Frame too short for extended payload length (126)" << std::endl;
      close(client_socket);
      server_thread.join();
      return;
    }
    payload_len = (static_cast<uint8_t>(response[2]) << 8) | static_cast<uint8_t>(response[3]);
    offset += 2;
  } else if (payload_len == 127) {
    if (recv_len < 10) {
      std::cerr << "[Test] Frame too short for extended payload length (127)" << std::endl;
      close(client_socket);
      server_thread.join();
      return;
    }
    payload_len = 0;
    for (int i = 0; i < 8; ++i) {
      payload_len = (payload_len << 8) | static_cast<uint8_t>(response[2 + i]);
    }
    offset += 8;
  }

  // Extract the payload
  if (recv_len < offset + payload_len) {
    std::cerr << "[Test] Frame too short for payload" << std::endl;
    close(client_socket);
    server_thread.join();
    return;
  }
  std::string echoed_message(response + offset, payload_len);

  std::cout << "[Test] Echoed message: " << echoed_message << std::endl;

  // Validate echoed message
  if (echoed_message != message) {
    std::cerr << "[Test] Echoed message does not match the sent message" << std::endl;
    close(client_socket);
    server_thread.join();
    return;
  }

  // Close the client socket
  close(client_socket);

  // Stop the server
  std::cout << "[Test] WebSocket server test completed" << std::endl;
  server_thread.join();
}

int main() {
  test_websocket_server();
  return 0;
}