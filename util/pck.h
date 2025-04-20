#pragma once

#include <iostream>
#include <string>

class pck {
public:
  std::string bl;
  std::string content_type;

  std::string status;
  std::string content;
  std::string headers;
  std::string body;
  std::string response;
  std::string cont_len;

  // std::bad_alloc

  //! this dont work

  pck(int status = 200) {
    std::cout << "[pck] Creating packet with status: " << status << std::endl;
    this->content_type = "Content-Type: text/plain\r\n";
    this->bl = "\r\n";
    this->status = "HTTP/1.1 " + std::to_string(status) + " OK" + bl;
    this->headers = content_type;
    this->content = "";
    this->response = "";
    this->cont_len = "";
  }

  void set_status(int status) {
    this->status = "HTTP/1.1 " + std::to_string(status) + " OK" + bl;
  }

  void add_header(std::string header) {
    headers += header + bl;
  }

  void set_content(std::string content) {
    this->content = content;
  }

  void add_con_cls() {
    headers += "Connection: close" + bl;
  }

  void set_cont_len() {
    cont_len = std::to_string(this->content.length());
  }
  void add_header_cont_len() {
    if (this->content.length() == 0) return;
    set_cont_len();
    headers += "Content-Length: " + cont_len + bl;
  }

  std::string export_(bool add_cl = false) {
    std::cout << "[pck] Exporting packet1" << std::endl;
    std::cout << "[pck] Content: " << content << std::endl;
    if (add_cl) add_header_cont_len();
    //response = status + headers + bl + content; // Export response
    response = 
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "\r\n"
      "Hello World!123";

    return response;
  }
};