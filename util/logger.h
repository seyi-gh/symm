#pragma once

#include <string>

class logger {
public:
  logger() = default;
  ~logger() = default;

  static std::string _deb(const std::string& state, const std::string& message, const std::string& function_name) {
    return "[" + state + "/" + function_name + "] " + message;
  }
  static std::string debug(const std::string& message, const std::string& function_name) {
    return _deb("debug", message, function_name);
  }
  static std::string error(const std::string& message, const std::string& function_name) {
    return _deb("error", message, function_name);
  }
  static std::string info(const std::string& message, const std::string& function_name) {
    return _deb("info", message, function_name);
  }
  static std::string warning(const std::string& message, const std::string& function_name) {
    return _deb("warning", message, function_name);
  }
};