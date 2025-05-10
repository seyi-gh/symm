#pragma once

#include <iostream>
#include <string>
#include <vector>

class PckParent {
public:
  std::string status_line;
  std::string content_type;
  std::string content_data;
  std::vector<std::string> headers;

  explicit PckParent() {};

  // Break line added
  void add_header(std::string header) {
    headers.push_back(header + breakline);
  }
  // Break line added
  void add_header(std::string header, std::string value) {
    headers.push_back(header_format(header, value));
  }
  // Break line added
  void set_content(std::string header = "Content-Type", std::string value = "text/html") {
    content_type = header_format(header, value);
  }

  void set_status(short int status_code) {
    status_line = format_status(status_code);
  }
  // Break line added
  void set_status(std::string status) {
    status_line = status + breakline;
  }
  void set_body(std::string body) {
    content_data = body;
  }

  std::string export_headers() {
    std::string headers_str;
    for (const auto& header : headers)
      headers_str += header;
    return headers_str;
  }

  std::string export_packet() {
    add_header("Content-Length", std::to_string(content_data.size()));
    std::string response = status_line + content_type + export_headers() + breakline + content_data;
    if (response.back() != '\n')
      response += breakline;
    return response;
  }

  PckParent& operator=(const PckParent& other) {
    if (this != &other) {
      status_line = other.status_line;
      content_type = other.content_type;
      content_data = other.content_data;
      headers = other.headers;
    }
    return *this;
  }

protected:
  const std::string breakline = "\r\n";
  virtual std::string format_status(short int status_code) const { return ""; };
  std::string header_format(std::string id, std::string value) const {
    return id + ": " + value + breakline;
  }
};

/*
#### Helper class for WebSocket responses
- Only coded to function in HTTP/1.1
- When exported discard length of content data
*/
class pck_WebSocket: public PckParent {
public:
  pck_WebSocket(short int status_code = -1): PckParent() {
    set_status(status_code);
  }

  std::string format_status(short int status_code = -1) const {
    if (status_code == -1) status_code = 500;
    if (status_code == 101)
      return "HTTP/1.1 101 Switching Protocols\r\n";
    else if (status_code == 400)
      return "HTTP/1.1 400 Bad Request\r\n";
    else if (status_code == 401)
      return "HTTP/1.1 401 Unauthorized\r\n";
    else if (status_code == 403)
      return "HTTP/1.1 403 Forbidden\r\n";
    else if (status_code == 405)
      return "HTTP/1.1 405 Method Not Allowed\r\n";
    else if (status_code == 426)
      return "HTTP/1.1 426 Upgrade Required\r\n";
    else if (status_code == 503)
      return "HTTP/1.1 503 Service Unavailable\r\n";
    return "HTTP/1.1 500 Internal Server Error\r\n";
  }
};

/*
class http_pck : public pck {
public:
  http_pck(short int status_code=-1)
    :pck(status_code) {
    if (status_code != -1) set_status(status_code);
  }

  std::string format_status(short int status_code) const override {
    if (status_code == 200)
      return "HTTP/1.1 200 OK\r\n";
    else if (status_code == 400)
      return "HTTP/1.1 400 Bad Request\r\n";
    else if (status_code == 401)
      return "HTTP/1.1 401 Unauthorized\r\n";
    else if (status_code == 403)
      return "HTTP/1.1 403 Forbidden\r\n";
    else if (status_code == 404)
      return "HTTP/1.1 404 Not Found\r\n";
    else if (status_code == 500)
      return "HTTP/1.1 500 Internal Server Error\r\n";
    return "HTTP/1.1 500 Internal Server Error\r\n";
  }
};
*/
