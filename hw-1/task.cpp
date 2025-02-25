#include "api.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

std::string getUniqueName() {
  static int counter = 1;
  return std::to_string(counter++);
};

enum class TokenType {
  NODE,
  END,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_CONCAT,
  TOK_OR,
  TOK_REPEAT,
  EMPTY,
};

struct Token {
  Token(TokenType type, std::string value) : type(type), value(value) {}

  TokenType type;
  std::string value;
};

struct BaseNode : std::enable_shared_from_this<BaseNode> {

  BaseNode(Token token, bool nullable = false)
      : token(token), nullable(nullable) {}

  void initializePositions() {
    if (!nullable) {
      auto self = shared_from_this();
      firstpos.push_back(self);
      lastpos.push_back(self);
    }
  }

  Token token;
  bool nullable;
  std::vector<std::shared_ptr<BaseNode>> firstpos{};
  std::vector<std::shared_ptr<BaseNode>> lastpos{};
};

struct OrNode : BaseNode {

  OrNode(std::shared_ptr<BaseNode> left, std::shared_ptr<BaseNode> right)
      : BaseNode(Token(TokenType::TOK_OR, "|"), true) {
    this->left = left;
    this->right = right;
    nullable = left->nullable || right->nullable;
    std::for_each(left->firstpos.begin(), left->firstpos.end(),
                  [this](std::shared_ptr<BaseNode> node) {
                    firstpos.push_back(node);
                    lastpos.push_back(node);
                  });
    std::for_each(right->firstpos.begin(), right->firstpos.end(),
                  [this](std::shared_ptr<BaseNode> node) {
                    firstpos.push_back(node);
                    lastpos.push_back(node);
                  });
  }

  std::shared_ptr<BaseNode> left;
  std::shared_ptr<BaseNode> right;
};

struct RepeatNode : BaseNode {

  RepeatNode(std::shared_ptr<BaseNode> repeatable)
      : BaseNode(Token(TokenType::TOK_REPEAT, "*"), true) {
    this->repeatable = repeatable;
    nullable = true;
    std::for_each(repeatable->firstpos.begin(), repeatable->firstpos.end(),
                  [this](std::shared_ptr<BaseNode> node) {
                    firstpos.push_back(node);
                    lastpos.push_back(node);
                  });
  }

  std::shared_ptr<BaseNode> repeatable;
};

struct ConcatNode : BaseNode {

  ConcatNode(std::shared_ptr<BaseNode> left, std::shared_ptr<BaseNode> right)
      : BaseNode(Token(TokenType::TOK_CONCAT, "*"), true) {
    this->left = left;
    this->right = right;
    nullable = left->nullable && right->nullable;
    std::for_each(left->firstpos.begin(), left->firstpos.end(),
                  [this, right](std::shared_ptr<BaseNode> node) {
                    firstpos.push_back(node);
                    if (right->nullable) {
                      lastpos.push_back(node);
                    }
                  });
    std::for_each(right->firstpos.begin(), right->firstpos.end(),
                  [this, left](std::shared_ptr<BaseNode> node) {
                    if (left->nullable) {
                      firstpos.push_back(node);
                    }
                    lastpos.push_back(node);
                  });
  }

  std::shared_ptr<BaseNode> left;
  std::shared_ptr<BaseNode> right;
};

struct SymbolNode : BaseNode {
  SymbolNode(std::string name)
      : BaseNode(Token(TokenType::NODE, name), false) {}
};

struct EmptyNode : BaseNode {
  EmptyNode() : BaseNode(Token(TokenType::EMPTY, "EMPTY"), true) {}
};

struct Preprocessor {

  std::string input;
  using resultT = std::vector<Token>;
  resultT result{};

  Preprocessor(const std::string &input) : input(input) {}

  resultT preprocess() {
    for (int i = 0; i < input.size(); ++i) {
      // Stack concationation into one token
      // TODO: Handle repeat
      if (std::isalnum(input[i])) {
        result.push_back(Token(TokenType::NODE, getUniqueName()));
        if (i + 1 < input.size() &&
            (std::isalnum(input[i + 1]) || input[i + 1] == '(')) {
          result.push_back(Token(TokenType::TOK_CONCAT, "."));
        }
      } else if (input[i] == '(') {
        result.push_back(Token(TokenType::TOK_LPAREN, "("));
      } else if (input[i] == ')') {
        result.push_back(Token(TokenType::TOK_RPAREN, ")"));
        if (i + 1 < input.size() &&
            (std::isalnum(input[i + 1]) || input[i + 1] == '(')) {
          result.push_back(Token(TokenType::TOK_CONCAT, "."));
        }
      } else if (input[i] == '|') {
        result.push_back(Token(TokenType::TOK_OR, "|"));
      } else if (input[i] == '*') {
        result.push_back(Token(TokenType::TOK_REPEAT, "*"));
      }
    }
    // Add the marker of the end to the regular expression
    result.push_back(Token(TokenType::END, "#"));
    return result;
  }
};

struct Parser {

  std::vector<Token> input;
  using iterType = std::vector<Token>::iterator;
  std::shared_ptr<BaseNode> resultRoot = nullptr;
  iterType cursor;

  Parser(std::vector<Token> tokens) : input(tokens), cursor(input.begin()) {}

  std::shared_ptr<BaseNode> parseOr() {
    auto res = parseConcat();
    while (cursor != input.end() && cursor->type == TokenType::TOK_OR) {
      ++cursor;
      res = std::make_shared<OrNode>(res, parseConcat());
      res->initializePositions();
    }
    return res;
  }

  std::shared_ptr<BaseNode> parseConcat() {
    auto res = parseRepeat();
    while (cursor != input.end() && cursor->type == TokenType::TOK_CONCAT) {
      ++cursor;
      res = std::make_shared<ConcatNode>(res, parseRepeat());
      res->initializePositions();
    }
    return res;
  }

  std::shared_ptr<BaseNode> parseRepeat() {
    auto res = parsePrimary();
    while (cursor != input.end() && cursor->type == TokenType::TOK_REPEAT) {
      ++cursor;
      res = std::make_shared<RepeatNode>(res);
      res->initializePositions();
    }
    return res;
  }

  std::shared_ptr<BaseNode> parsePrimary() {
    if (cursor->type == TokenType::TOK_LPAREN) {
      ++cursor;
      std::shared_ptr<BaseNode> res = parseOr();
      if (cursor != input.end() && cursor->type == TokenType::TOK_RPAREN) {
        ++cursor;
      }
      return res;
    }
    if (cursor != input.end() && cursor->type == TokenType::NODE) {
      auto res = std::make_shared<SymbolNode>(cursor->value);
      res->initializePositions();
      ++cursor;
      return res;
    }
    if (cursor != input.end() && cursor->type == TokenType::END) {
      auto res = std::make_shared<SymbolNode>(cursor->value);
      res->initializePositions();
      return res;
    }
    // There is no self positions for EmptyNode
    return std::make_shared<EmptyNode>();
  }

  std::shared_ptr<BaseNode> parse() {
    resultRoot = parseOr();
    return resultRoot;
  }
};

DFA re2dfa(const std::string &s) {
  DFA res = DFA(Alphabet(s));

  res.create_state("Start", true);
  res.set_initial("Start");

  Preprocessor preprocessor(s);
  std::vector<Token> tokens = preprocessor.preprocess();

#ifdef DEBUG
  std::for_each(tokens.begin(), tokens.end(),
                [&res](Token t) { std::cout << t.value << " "; });
  std::cout << std::endl;
#endif

  Parser parser(tokens);
  std::shared_ptr<BaseNode> root = parser.parse();

  return res;
}
