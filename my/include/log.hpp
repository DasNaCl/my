#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Logger;

class MessageCollector
{
public:
  friend class Logger;
  friend const MessageCollector& operator<<(const MessageCollector& lg, const char* message);
  friend const MessageCollector& operator<<(const MessageCollector& lg, const std::string& message);
  ~MessageCollector();
private:
  MessageCollector(Logger& log);

  Logger& log;
  mutable std::string message;
};

MessageCollector emit_error(const std::string& module, std::uint_fast32_t column, std::uint_fast32_t row);
MessageCollector emit_info(const std::string& module, std::uint_fast32_t column, std::uint_fast32_t row);
MessageCollector emit_warn(const std::string& module, std::uint_fast32_t column, std::uint_fast32_t row);


