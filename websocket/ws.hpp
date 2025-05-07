#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set> // Added for std::unordered_set
#include <mutex>
#include <functional>
#include <queue> // Added for std::queue
#include <sys/epoll.h>
#include <condition_variable>
#include <thread>
#include <atomic>
#include "../util/logger.h"
#include "../util/pck.h"

class WebSocketServer {
public:
  using MessageHandler = std::function<void(int, const std::string&)>;
  using HandshakeValidator = std::function<bool(const std::string&)>;

  WebSocketServer(int port = -1, int max_threads = 4);
  virtual ~WebSocketServer();

  std::unordered_set<int> connected_clients_;
  std::mutex connected_clients_mutex_;

  void run(); // Start the server
  void stop(); // Stop the server
  void set_message_handler(MessageHandler handler); // Set custom message handler
  void set_handshake_validator(HandshakeValidator validator); // Set custom handshake validator

  void close_connection(int client_socket);

  bool is_socket_closed(int client_socket); // Check if a socket is closed

  void broadcast(const std::string& message);
  std::string receive_response();

  int port_;
  int server_fd_;
  int epoll_fd_;
  std::atomic<bool> running_;
  std::mutex client_mutex_;
  MessageHandler custom_message_handler_;
  HandshakeValidator custom_handshake_validator_;
  std::vector<std::thread> thread_pool_;
  std::queue<int> task_queue_;
  std::mutex queue_mutex_;
  std::condition_variable task_cv_;

  bool setup_server_socket(); // Setup the server socket
  void handle_events(); // Handle incoming events
  void worker_thread(); // Worker thread for handling events
  void handle_client_read(int client_socket); // Handle client read events
  void handle_client_write(int client_socket, const std::string& message); // Handle client write events
  bool perform_handshake(int client_socket); // Perform WebSocket handshake

  bool decode_frame(const std::vector<uint8_t>& frame, std::string& payload); // Decode WebSocket frame
  std::vector<uint8_t> encode_frame(const std::string& payload); // Encode WebSocket frame
  bool is_valid_utf8(const std::string& str); // Validate UTF-8 encoding
  std::string sanitize_utf8(const std::string& str); // Sanitize UTF-8 strings
  std::string compute_accept_key(const std::string& sec_websocket_key);
  std::string extract_header(const std::string& request, const std::string& header_name);

  virtual bool validate_handshake(const std::string& request); // Override for custom handshake validation
  virtual void on_message(int client_socket, const std::string& message); // Override for custom message handling
  virtual std::string process_data(const std::string& data); // Process data before sending
  std::unordered_set<int> closed_sockets_; 
  std::mutex close_sockets_mutex_;

  std::mutex response_mutex_;
  std::condition_variable response_cv_;
  std::queue<std::string> response_queue_;

protected:


private:

};