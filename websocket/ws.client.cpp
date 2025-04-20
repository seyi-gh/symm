#include "ws.client.hpp"

#include <iostream>
#include <sstream>
#include <random>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

std::string wsutils::generate_sec_ws_key() {
  unsigned char random_bytes[16];
  RAND_bytes(random_bytes, sizeof(random_bytes));
    
  BIO* bio = BIO_new(BIO_s_mem());
  BIO* b64 = BIO_new(BIO_f_base64());
  bio = BIO_push(b64, bio);
  BIO_write(bio, random_bytes, sizeof(random_bytes));
  BIO_flush(bio);

  BUF_MEM* buffer_ptr;
  BIO_get_mem_ptr(bio, &buffer_ptr);
  std::string key(buffer_ptr->data, buffer_ptr->length - 1); // Exclude newline

  BIO_free_all(bio);
  return key;
}

std::ostringstream wsutils::generate_headers(const std::vector<std::string> headers) {
  std::ostringstream oss;
  for (const auto& header : headers) {
    oss << header << "\r\n";
  }
  oss << "\r\n";
  return oss;
}

WebSocketClient::WebSocketClient(const std::string& host, int port)
  : host_(host), port_(port), socket_fd_(-1), connected_(false) {}

WebSocketClient::~WebSocketClient() { 
  if (socket_fd_ != -1) close(socket_fd_); 
}

bool WebSocketClient::connect() {
  if (connected_) {
    std::cerr << "[connect] Already connected" << std::endl;
    return false;
  }

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &res) != 0) {
    std::cerr << "[connect] Failed to resolve host" << std::endl;
    return false;
  }

  socket_fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (socket_fd_ < 0) {
    perror("[connect] Socket creation failed");
    freeaddrinfo(res);
    return false;
  }

  if (::connect(socket_fd_, res->ai_addr, res->ai_addrlen) < 0) {
    perror("[connect] Connection failed");
    close(socket_fd_);
    freeaddrinfo(res);
    return false;
  }

  freeaddrinfo(res);

  if (!perform_handshake()) {
    close(socket_fd_);
    socket_fd_ = -1;
    return false;
  }

  connected_ = true;
  std::cout << "[WebSocket] Connected to " << host_ << ":" << port_ << std::endl;
  return true;
}

bool WebSocketClient::perform_handshake() {
  std::string key = wsutils::generate_sec_ws_key();
  std::cout << "[perform_handshake] Generated random key: " << key << std::endl;

  // Build the handshake request carefully
  std::ostringstream request;
  request << "GET / HTTP/1.1\r\n";
  request << "Host: " << host_;
  if (port_ != 80 && port_ != 443) {
    request << ":" << port_;
  }
  request << "\r\n";
  request << "Upgrade: websocket\r\n";
  request << "Connection: Upgrade\r\n";
  request << "Sec-WebSocket-Key: " << key << "\r\n";
  request << "Sec-WebSocket-Version: 13\r\n";
  request << "\r\n";  // End of headers

  std::string request_str = request.str();
  std::cout << "Sending handshake request:\n" << request_str << std::endl;

  // Send the raw request directly (don't use the send() method which adds WebSocket framing)
  if (::send(socket_fd_, request_str.c_str(), request_str.size(), 0) <= 0) {
    perror("[perform_handshake] Failed to send handshake request");
    return false;
  }

  // Read response
  char buffer[4096];
  int len = recv(socket_fd_, buffer, sizeof(buffer) - 1, 0);
  if (len <= 0) {
    perror("[perform_handshake] Failed to receive handshake response");
    return false;
  }

  buffer[len] = '\0';
  std::string response(buffer);
  std::cout << "Received handshake response:\n" << response << std::endl;

  // Check for successful handshake
  if (response.find("HTTP/1.1 101 Switching Protocols") == std::string::npos) {
    std::cerr << "[perform_handshake] Handshake failed - expected 101 response" << std::endl;
    return false;
  }

  // Verify the accept key (optional but recommended)
  std::string accept_key = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(accept_key.c_str()), accept_key.size(), hash);

  BIO* bio = BIO_new(BIO_s_mem());
  BIO* b64 = BIO_new(BIO_f_base64());
  bio = BIO_push(b64, bio);
  BIO_write(bio, hash, SHA_DIGEST_LENGTH);
  BIO_flush(bio);

  BUF_MEM* buffer_ptr;
  BIO_get_mem_ptr(bio, &buffer_ptr);
  std::string expected_accept(buffer_ptr->data, buffer_ptr->length - 1);
  BIO_free_all(bio);

  if (response.find("Sec-WebSocket-Accept: " + expected_accept) == std::string::npos) {
    std::cerr << "[perform_handshake] Invalid Sec-WebSocket-Accept header" << std::endl;
    return false;
  }

  return true;
}

bool WebSocketClient::send(const std::vector<uint8_t>& data, bool force) {
  if (!connected_ && !force) {
    std::cerr << "[send] Not connected to server" << std::endl;
    return false;
  }

  if (data.empty()) {
    std::cerr << "[send] No data to send" << std::endl;
    return false;
  }

  // Prepare header
  std::vector<uint8_t> frame;
  frame.push_back(0x81); // FIN + text frame (RSV bits all 0)

  size_t len = data.size();
  if (len <= 125) {
    frame.push_back(len | 0x80); // Mask bit set
  } else if (len <= 65535) {
    frame.push_back(126 | 0x80);
    frame.push_back((len >> 8) & 0xFF);
    frame.push_back(len & 0xFF);
  } else {
    frame.push_back(127 | 0x80);
    for (int i = 7; i >= 0; i--) {
      frame.push_back((len >> (i * 8)) & 0xFF);
    }
  }

  // Add mask
  uint8_t mask[4];
  RAND_bytes(mask, sizeof(mask));
  frame.insert(frame.end(), mask, mask + 4);

  // Mask payload
  std::vector<uint8_t> masked_data(data.size());
  for (size_t i = 0; i < data.size(); ++i) {
    masked_data[i] = data[i] ^ mask[i % 4];
  }
  frame.insert(frame.end(), masked_data.begin(), masked_data.end());

  std::cout << "Sending frame: ";
  for (auto b : frame) {
    printf("%02x ", b);
  }
  std::cout << std::endl;

  // Send entire frame at once
  ssize_t sent = ::send(socket_fd_, frame.data(), frame.size(), 0);
  if (sent <= 0) {
    perror("[send] Failed to send data");
    return false;
  }

  std::cout << "[send] Sent " << sent << " bytes (payload: " << data.size() << " bytes)" << std::endl;
  return true;
}

bool WebSocketClient::send(const std::vector<uint8_t>& data) {
  return send(data, false); // Call the overloaded method with force set to false
}

bool WebSocketClient::receive(std::vector<uint8_t>& data) {
  uint8_t header[2];
  ssize_t header_len = recv(socket_fd_, header, 2, 0);
  if (header_len == 0) {
    std::cerr << "[receive] Connection closed by server" << std::endl;
    return false;
  } else if (header_len < 0) {
    perror("[receive] Failed to read header");
    return false;
  }

  size_t len = header[1] & 0x7F;
  if (len == 126) {
    uint8_t ext[2];
    if (recv(socket_fd_, ext, 2, 0) <= 0) {
      perror("[receive] Failed to read extended payload length (126)");
      return false;
    }
    len = (ext[0] << 8) | ext[1];
  } else if (len == 127) {
    uint8_t ext[8];
    if (recv(socket_fd_, ext, 8, 0) <= 0) {
      perror("[receive] Failed to read extended payload length (127)");
      return false;
    }
    len = 0;
    for (int i = 0; i < 8; ++i) {
      len = (len << 8) | ext[i];
    }
  }

  // Validate payload length
  if (len > 65535) { // Arbitrary limit to prevent excessive memory allocation
    std::cerr << "[receive] Payload length too large: " << len << std::endl;
    return false;
  }

  // Check if the mask bit is set (it should not be for server responses)
  bool is_masked = header[1] & 0x80;
  uint8_t mask[4] = {0};

  if (is_masked) {
    if (recv(socket_fd_, mask, 4, 0) <= 0) {
      perror("[receive] Failed to read mask");
      return false;
    }
  }

  data.resize(len);
  ssize_t payload_len = recv(socket_fd_, data.data(), len, 0);
  if (payload_len == 0) {
    std::cerr << "[receive] Connection closed by server while reading payload" << std::endl;
    return false;
  } else if (payload_len < 0) {
    perror("[receive] Failed to read payload");
    return false;
  }

  // Unmask the payload if it is masked
  if (is_masked) {
    for (size_t i = 0; i < len; ++i) {
      data[i] ^= mask[i % 4];
    }
  }

  std::cout << "[receive] Received " << len << " bytes of data" << std::endl;
  return true;
}

bool WebSocketClient::is_connected() const {
  return connected_;
}