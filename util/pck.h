#pragma once

#include <iostream>
#include <string>

class pck {
private:
  const std::string content_type = "Content-Type: text/plain" + bl;
  void set_cont_len() {
    cont_len = std::to_string(this->content.length());
  }
  void add_header_cont_len() {
    if (this->content.length() == 0) return;
    set_cont_len();
    headers += "Content-Length: " + cont_len + bl;
  }

public:
  const std::string bl = "\r\n";

  std::string status;
  std::string content;
  std::string headers;
  std::string body;
  std::string response;
  std::string cont_len;

  pck(int status=200) {
    set_status(status);
    add_header(content_type);
  }

  void set_status(int status) {
    this->status = "HTTP/1.1 " + std::to_string(status) + " OK" + bl;
  }

  void add_header(const std::string& header) {
    headers += header + bl;
  }

  void set_content(const std::string& content) {
    this->content = content;
  }

  void add_con_cls() {
    headers += "Connection: close" + bl;
  }

  std::string export_(bool add_cl = false) {
    if (add_cl) add_header_cont_len();
    response = status + headers + bl + content;
    return response;
  }
};