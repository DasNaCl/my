#include <parser.hpp>
#include <tokenizer.hpp>
#include <log.hpp>

#include <tsl/bhopscotch_set.h>

struct Parser
{
public:
  Parser(Tokenizer& tokenizer)
    : tokenizer(tokenizer), ast()
  {
    next_token(); next_token(); next_token();
  }

  std::vector<Statement::Ptr> parse() &&
  {
    if(peek(TokenKind::EndOfFile))
      return {};

    while(!peek(TokenKind::EndOfFile))
    {
      ast.emplace_back(parse_root());
    }
    return std::move(ast);
  }

  MessageCollector emit_error()
  {
    breakpoint();
    return ::emit_error(tokenizer.module_name(), current_token.loc().column_beg, current_token.loc().row_beg);
  }

  void skip_to_next_toplevel()
  {
    bool another_fn = (peek(TokenKind::Id)     && n1_peek(TokenKind::Equal))
                   || (peek(TokenKind::Equal));
    while(!another_fn && !peek(TokenKind::EndOfFile))
    {
      while(!peek(TokenKind::EndOfFile))
      {
        next_token();
      }
      next_token();

      another_fn = (peek(TokenKind::Id)     && n1_peek(TokenKind::Equal))
                || (peek(TokenKind::Equal));
    }
  }

  ErrorExpression::Ptr error_expr(SourceRange range)
  { return std::make_shared<ErrorExpression>(range); } // TODO: Add more error context info
  ErrorExpression::Ptr error_expr()
  { return std::make_shared<ErrorExpression>(current_token.loc()); } // TODO: Add more error context info

  ErrorStatement::Ptr error_stmt()
  { return std::make_shared<ErrorStatement>(current_token.loc()); } // TODO: Add more error context info
  ErrorStatement::Ptr error_stmt(SourceRange range)
  { range.widen(prev_tok_loc); return std::make_shared<ErrorStatement>(range); } // TODO: Add more error context info
private:
  void next_token()
  {
    prev_tok_loc = current_token.loc();

    current_token = lookahead.first;
    lookahead.first = lookahead.second;
    lookahead.second = tokenizer.get();
  }
  
  bool peek(TokenKind kind) const
  {
    if(kind == TokenKind::Id && current_token.tok_kind() == TokenKind::Id)
    {
      // see if the identifier is a already known subtree
      const char* text = current_token.data_as_text();
      return trees.find(Symbol(text)) == trees.end();
    }
    return current_token.tok_kind() == kind;
  }

  bool n1_peek(TokenKind kind) const
  {
    if(kind == TokenKind::Id && lookahead.first.tok_kind() == TokenKind::Id)
    {
      // see if the identifier is a already known subtree
      const char* text = lookahead.first.data_as_text();
      return trees.find(Symbol(text)) == trees.end();
    }
    return lookahead.first.tok_kind() == kind;
  }

  bool n2_peek(TokenKind kind) const
  {
    if(kind == TokenKind::Id && lookahead.second.tok_kind() == TokenKind::Id)
    {
      // see if the identifier is a already known subtree
      const char* text = lookahead.second.data_as_text();
      return trees.find(Symbol(text)) == trees.end();
    }
    return lookahead.second.tok_kind() == kind;
  }

  bool accept(TokenKind kind)
  {
    if(peek(kind))
    {
      next_token();
      return true;
    }
    return false;
  }

  bool expect(TokenKind kind)
  {
    if(!accept(kind))
    {
      emit_error() << "Expected token \"" << to_string(kind) << "\" but got \"" << to_string(current_token.tok_kind()) << "\".";

      return false;
    }
    return true;
  }

  bool expect_or(TokenKind kind0, TokenKind kind1)
  {
    if(!accept(kind0) && !accept(kind1))
    {
      emit_error() << "Expected either token \"" << to_string(kind0) << "\" or \"" << to_string(kind1) << "\" but got \""
                   << to_string(current_token.tok_kind()) << "\".";
      return false;
    }
    return true;
  }
private:
  Tokenizer& tokenizer;
  std::vector<Statement::Ptr> ast;

  Token current_token;
  std::pair<Token, Token> lookahead;

  SourceRange prev_tok_loc;

  SymbolMap<Expression::Ptr> trees;
private:
  // parsers
  
  Statement::Ptr parse_root()
  { return parse_definition(); }

  Statement::Ptr parse_definition()
  {
    auto range = current_token.loc();

    Expression::Ptr name;
    if(peek(TokenKind::Id))
      name = parse_identifier();
    accept(TokenKind::Equal);
    auto body = parse_expression();

    expect_or(TokenKind::Semicolon, TokenKind::EndOfFile);

    if(auto is_id = std::dynamic_pointer_cast<Identifier>(name))
      trees[is_id->id()] = body->clone();
    range.widen(prev_tok_loc);
    return std::make_shared<Definition>(range, name, body);
  }

  Expression::Ptr parse_identifier()
  {
    auto range = current_token.loc();
    const char* data = current_token.data_as_text();

    if(!expect(TokenKind::Id))
      return error_expr();
    return std::make_shared<Identifier>(range, Symbol(data));
  }

  Expression::Ptr parse_fn(bool require_lambda = true)
  {
    SourceRange range = current_token.loc();
    if(require_lambda) expect(TokenKind::Lambda);
    auto var = parse_identifier();
    expect(TokenKind::Dot);

    auto body = parse_expression();

    range.widen(prev_tok_loc);
    if(auto is_id = std::dynamic_pointer_cast<Identifier>(var))
      return std::make_shared<Lambda>(range, is_id, body);
    return error_expr(range);
  }

  std::int_fast32_t lbp(TokenKind kind)
  {
    switch(kind)
    {
    default: return 0;

    case TokenKind::LParen: return 50;
    case TokenKind::Id: return 100;
    }
  }

  Expression::Ptr nud(Token tok)
  {
    switch(tok.tok_kind())
    {
    default: return nullptr;

    case TokenKind::LParen: {
                              auto body = parse_expression();
                              expect(TokenKind::RParen);
                              return body;
                            };
    case TokenKind::Lambda: return parse_fn(false);
    case TokenKind::Id:     {
                              auto it = trees.find(Symbol(tok.data_as_text()));
                              if(it != trees.end())
                                return it->second->clone();
                              return std::make_shared<Identifier>(tok.loc(), tok.data_as_text());
                            }
    }
  }

  Expression::Ptr led(Token tok, Expression::Ptr left)
  {
    switch(tok.tok_kind())
    {
    default: return nullptr;

    case TokenKind::LParen:
       {
         auto range = left->source_range();
         auto right = parse_expression();
         expect(TokenKind::RParen);
         range.widen(prev_tok_loc);
         return std::make_shared<FunctionCall>(range, left, right);
       }

    case TokenKind::Id:
       {
         auto it = trees.find(Symbol(tok.data_as_text()));
         auto right = it != trees.end() ? it->second->clone() : std::make_shared<Identifier>(tok.loc(), tok.data_as_text());
         auto range = left->source_range();
         range.widen(right->source_range());
         return std::make_shared<FunctionCall>(range, left, right);
       }
    }
  }

  Expression::Ptr parse_expression(std::int_fast32_t prec = 0)
  {
    auto t = current_token;
    next_token();
    Expression::Ptr left = nud(t);
    while(prec < lbp(current_token.tok_kind()))
    {
      t = current_token;
      next_token();
      left = led(t, left);
    }
    return left;
  }
};

std::vector<Statement::Ptr> parse(Tokenizer& tokenizer)
{
  return Parser(tokenizer).parse();
}

