#pragma once

#include <iostream>
#include <string>
#include <ctime>
#include <sstream>

#define outputfunc std::cout
#define outputfucerr std::cerr
#define d_endl std::endl
#define log_path "./logs/"

class logger {
public:
  logger() = default;
  ~logger() = default;

  static void log(
    const std::string& log_name,
    const std::string& message,
    const std::string& function_name) 
  {
    auto t = std::time(nullptr);
    std::tm tm;
    std::tm* tm_ptr = std::localtime(&t);
    if (tm_ptr) {
      tm = *tm_ptr;
    }
    char time_buffer[20];
    std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &tm);
    std::ostringstream log_stream;
    log_stream << time_buffer << " [" << log_name << "][" << function_name << "] " << message;
    outputfunc << log_stream.str() << d_endl;
  }

  static void debug(
    const std::string& message,
    const std::string& function_name) 
  {
    log(__func__, message, function_name);
  }
  static void info(
    const std::string& message,
    const std::string& function_name)
  {
    log(__func__, message, function_name);
  }
  static void warn(
    const std::string& message,
    const std::string& function_name)
  {
    log(__func__, message, function_name);
  }
  static void error(
    const std::string& message,
    const std::string& function_name)
  {
    log(__func__, message, function_name);
  }
};