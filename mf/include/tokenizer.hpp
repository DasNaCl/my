#pragma once

#include <tsl/hopscotch_map.h>

#include <SourceRange.hpp>

#include <cstdint>
#include <istream>
#include <memory>
#include <string>

enum class TokenKind : std::int_fast16_t
{
#define TOK(x) x,
#define TOK_CONTROL(x, v) x = v,
#define TOK_EXPR_OP(x, v) x = v,

#include "tokens.def"
};

struct Token
{
  SourceRange range;
  TokenKind kind;
  void* data;

  Token() = default;
  Token(SourceRange range, TokenKind kind)
    : range(range), kind(kind)
  {  }
  Token(SourceRange range, TokenKind kind, void* data)
    : range(range), kind(kind), data(data)
  {  }

  explicit operator std::string() const;

  friend std::ostream& operator<<(std::ostream& os, const Token& tok);
};

class Tokenizer
{
public:
  Tokenizer(const char* module, std::istream& handle);

  Token get();

  const std::string& module_name() const;
private:
  char read(); 

private:
  const std::string module;
  std::istream& handle;
  std::string linebuf;
  std::size_t row;
  std::size_t col; 

  tsl::hopscotch_map<std::uint_fast32_t, std::vector<std::uint_fast8_t>> token_data_table;
};

