#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "api.hpp"

void log(const std::string &str) {
#ifdef DEBUG
  std::cout << str << std::endl;
#endif
}

constexpr char SYMBOL_OR = '|';
constexpr char SYMBOL_CONCAT = '.';
constexpr char SYMBOL_REPEAT = '*';
constexpr char SYMBOL_LPAREN = '(';
constexpr char SYMBOL_RPAREN = ')';
constexpr char SYMBOL_EMPTY = '{';
constexpr char SYMBOL_NUMBER_SIGN = '#';
constexpr char SYMBOL_HELPER_POSITION = '?';
constexpr char SYMBOL_ROOT = '@';

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
  Token(TokenType type, char value) : type(type), value(value) {}

  TokenType type;
  char value;
};

struct PositionNode;

struct BaseNode {

  BaseNode(Token token, bool nullable = false)
      : token(token), nullable(nullable) {}

  virtual ~BaseNode() {}

  Token token;
  bool nullable;
  std::vector<std::shared_ptr<BaseNode>> firstpos{};
  std::vector<std::shared_ptr<BaseNode>> lastpos{};
  std::vector<std::shared_ptr<BaseNode>> followpos{};
};

struct OrNode : BaseNode {

  OrNode(std::shared_ptr<BaseNode> left, std::shared_ptr<BaseNode> right)
      : BaseNode(Token(TokenType::TOK_OR, SYMBOL_OR), true) {
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

  virtual ~OrNode(){};

  std::shared_ptr<BaseNode> left;
  std::shared_ptr<BaseNode> right;
};

struct RepeatNode : BaseNode {

  RepeatNode(std::shared_ptr<BaseNode> repeatable)
      : BaseNode(Token(TokenType::TOK_REPEAT, SYMBOL_REPEAT), true) {
    this->repeatable = repeatable;
    nullable = true;
    std::for_each(repeatable->firstpos.begin(), repeatable->firstpos.end(),
                  [this](std::shared_ptr<BaseNode> node) {
                    firstpos.push_back(node);
                    lastpos.push_back(node);
                  });
    std::for_each(repeatable->lastpos.begin(), repeatable->lastpos.end(),
                  [this, &repeatable](std::shared_ptr<BaseNode> node) {
                    std::for_each(repeatable->firstpos.begin(),
                                  repeatable->firstpos.end(),
                                  [this, &node](std::shared_ptr<BaseNode> n) {
                                    node->followpos.push_back(n);
                                  });
                  });
  }

  virtual ~RepeatNode() {}

  std::shared_ptr<BaseNode> repeatable;
};

struct ConcatNode : BaseNode {

  ConcatNode(std::shared_ptr<BaseNode> left, std::shared_ptr<BaseNode> right)
      : BaseNode(Token(TokenType::TOK_CONCAT, SYMBOL_CONCAT), true) {
    this->left = left;
    this->right = right;
    nullable = left->nullable && right->nullable;
    std::for_each(left->firstpos.begin(), left->firstpos.end(),
                  [this, &right](std::shared_ptr<BaseNode> node) {
                    firstpos.push_back(node);
                    if (right->nullable) {
                      lastpos.push_back(node);
                    }
                  });
    std::for_each(right->firstpos.begin(), right->firstpos.end(),
                  [this, &left](std::shared_ptr<BaseNode> node) {
                    if (left->nullable) {
                      firstpos.push_back(node);
                    }
                    lastpos.push_back(node);
                  });
    std::for_each(left->lastpos.begin(), left->lastpos.end(),
                  [this, &right](std::shared_ptr<BaseNode> node) {
                    std::for_each(right->firstpos.begin(),
                                  right->firstpos.end(),
                                  [this, &node](std::shared_ptr<BaseNode> n) {
                                    node->followpos.push_back(n);
                                  });
                  });
  }

  virtual ~ConcatNode() {}

  std::shared_ptr<BaseNode> left;
  std::shared_ptr<BaseNode> right;
};

std::vector<std::shared_ptr<PositionNode>> positionsGlobal{};
std::unordered_map<char, std::vector<std::shared_ptr<PositionNode>>>
    symbolToPositions{};

int generateReadableNum() {
  static int num = 1;
  return num++;
}

struct PositionNode : BaseNode, std::enable_shared_from_this<PositionNode> {
  PositionNode(char name = SYMBOL_HELPER_POSITION, bool nullable = false)
      : BaseNode(Token(TokenType::NODE, name), nullable), name(name) {}

  virtual ~PositionNode() {}

  void initializePositions() {
    auto self = shared_from_this();
    firstpos.push_back(self);
    lastpos.push_back(self);
    positionsGlobal.push_back(self);
    if (name != SYMBOL_HELPER_POSITION) {
      if (symbolToPositions.find(name) == symbolToPositions.end()) {
        symbolToPositions[name] = {self};
      } else {
        symbolToPositions[name].push_back(self);
      }
    }
  }

  std::string getFollowPosReadable() {
    if (!conditionName.empty()) {
      return conditionName;
    }
    std::string conditionName = "";
    for (auto pos : followpos) {
      auto position = dynamic_cast<PositionNode *>(pos.get());
      conditionName += std::to_string(position->readableNum);
      if (pos != followpos.back()) {
        conditionName += ".";
      }
    }
    return conditionName;
  }

  char name;
  std::string conditionName{};
  int readableNum = generateReadableNum();
};

struct EmptyNode : PositionNode {
  EmptyNode() : PositionNode(SYMBOL_EMPTY, true) {}

  virtual ~EmptyNode() {}
};

struct Preprocessor {

  std::string input;
  using resultT = std::vector<Token>;
  resultT result{};

  Preprocessor(const std::string &input) : input(input) {}

  resultT preprocess() {
    input += "#";
    for (int i = 0; i < input.size(); ++i) {
      // Stack concationation into one token
      // TODO: Handle repeat
      if (std::isalnum(input[i])) {
        result.push_back(Token(TokenType::NODE, input[i]));
        if (i + 1 < input.size() &&
            (std::isalnum(input[i + 1]) || input[i + 1] == SYMBOL_LPAREN ||
             input[i + 1] == SYMBOL_NUMBER_SIGN)) {
          result.push_back(Token(TokenType::TOK_CONCAT, SYMBOL_CONCAT));
        }
      } else if (input[i] == '(') {
        result.push_back(Token(TokenType::TOK_LPAREN, SYMBOL_LPAREN));
      } else if (input[i] == ')') {
        result.push_back(Token(TokenType::TOK_RPAREN, SYMBOL_RPAREN));
        if (i + 1 < input.size() &&
            (std::isalnum(input[i + 1]) || input[i + 1] == SYMBOL_LPAREN ||
             input[i + 1] == SYMBOL_NUMBER_SIGN)) {
          result.push_back(Token(TokenType::TOK_CONCAT, SYMBOL_CONCAT));
        }
      } else if (input[i] == '|') {
        result.push_back(Token(TokenType::TOK_OR, SYMBOL_OR));
      } else if (input[i] == '*') {
        result.push_back(Token(TokenType::TOK_REPEAT, SYMBOL_REPEAT));
        if (i + 1 < input.size() &&
            (std::isalnum(input[i + 1]) || input[i + 1] == SYMBOL_LPAREN ||
             input[i + 1] == SYMBOL_NUMBER_SIGN)) {
          result.push_back(Token(TokenType::TOK_CONCAT, SYMBOL_CONCAT));
        }
      } else if (input[i] == '#') {
        result.push_back(Token(TokenType::END, SYMBOL_NUMBER_SIGN));
      }
    }
    return result;
  }
};

struct Parser {

  std::vector<Token> input;
  using iterType = std::vector<Token>::iterator;
  std::shared_ptr<BaseNode> resultRoot = nullptr;
  std::map<std::string, std::shared_ptr<BaseNode>> symbolsToNodes{};
  iterType cursor;

  Parser(std::vector<Token> tokens) : input(tokens), cursor(input.begin()) {}

  std::shared_ptr<BaseNode> parseOr() {
    auto res = parseConcat();
    while (cursor != input.end() && cursor->type == TokenType::TOK_OR) {
      ++cursor;
      res = std::make_shared<OrNode>(res, parseConcat());
    }
    return res;
  }

  std::shared_ptr<BaseNode> parseConcat() {
    auto res = parseRepeat();
    while (cursor != input.end() && cursor->type == TokenType::TOK_CONCAT) {
      ++cursor;
      res = std::make_shared<ConcatNode>(res, parseRepeat());
    }
    return res;
  }

  std::shared_ptr<BaseNode> parseRepeat() {
    auto res = parsePrimary();
    while (cursor != input.end() && cursor->type == TokenType::TOK_REPEAT) {
      ++cursor;
      res = std::make_shared<RepeatNode>(res);
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
      auto res = std::make_shared<PositionNode>(cursor->value);
      res->initializePositions();
      ++cursor;
      return res;
    }
    if (cursor != input.end() && cursor->type == TokenType::END) {
      auto res = std::make_shared<PositionNode>(cursor->value);
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

std::shared_ptr<PositionNode>
getNotMarked(const std::vector<std::shared_ptr<PositionNode>> &positions,
             const std::vector<std::shared_ptr<PositionNode>> &marked) {
  std::shared_ptr<PositionNode> result = nullptr;

  std::for_each(positions.begin(), positions.end(),
                [&result, &marked](std::shared_ptr<PositionNode> node) {
                  if (std::find(marked.begin(), marked.end(), node) ==
                      marked.end()) {
                    result = node;
                    return;
                  }
                });

  return result;
}

DFA re2dfa(const std::string &s) {

  auto alpabet = Alphabet(s);

  auto res = DFA(alpabet);

  Preprocessor preprocessor(s);
  auto tokens = preprocessor.preprocess();

#ifdef DEBUG
  std::for_each(tokens.begin(), tokens.end(),
                [&res](Token t) { std::cout << t.value << " "; });
  std::cout << std::endl;
#endif

  Parser parser(tokens);
  auto root = parser.parse();

  log("Expression parsed...");

  res.create_state("root", true);
  res.set_initial("root");

  auto R = std::make_shared<PositionNode>();
  std::for_each(
      root->firstpos.begin(), root->firstpos.end(),
      [&R](std::shared_ptr<BaseNode> node) { R->followpos.push_back(node); });

  log("Root position condition: " + R->getFollowPosReadable());

  std::vector<std::shared_ptr<PositionNode>> Q{};
  Q.push_back(R);
  std::vector<std::shared_ptr<PositionNode>> marked{};

  int cycle = 1;

  while (R != nullptr) {

    log("\nCycle " + std::to_string(cycle) + ". R: " + R->getFollowPosReadable());

    marked.push_back(R);

    std::for_each(alpabet.begin(), alpabet.end(), [&R, &Q, &res](char c) {
      PositionNode S{};

      std::for_each(symbolToPositions[c].begin(), symbolToPositions[c].end(),
                    [&R, &S](std::shared_ptr<PositionNode> node) {
                      if (std::find(R->followpos.begin(), R->followpos.end(),
                                    node) != R->followpos.end()) {
                        for (auto it = node->followpos.begin();
                             it != node->followpos.end(); ++it) {
                          S.followpos.push_back(*it);
                        }
                      }
                    });

      if (S.followpos.size() == 0) {
        return;
      }

      std::string log_str{};
      log_str += "Symbol " + std::to_string(c) +
                 ". S:" + S.getFollowPosReadable() + ". Q:";
      std::for_each(Q.begin(), Q.end(),
                    [&log_str](std::shared_ptr<PositionNode> node) {
                      log_str += " " + node->getFollowPosReadable();
                    });
      log(log_str);

      // S not in Q
      bool SinQ = false;
      for (auto qIt = Q.begin(); qIt != Q.end(); ++qIt) {
        if ((*qIt)->followpos.size() != S.followpos.size()) {
          continue;
        }

        int foundCnt = 0;
        for (auto qFollowposIt = (*qIt)->followpos.begin();
             qFollowposIt != (*qIt)->followpos.end(); ++qFollowposIt) {
          for (auto sFollowposIt = S.followpos.begin();
               sFollowposIt != S.followpos.end(); ++sFollowposIt) {
            if ((*qFollowposIt) == (*sFollowposIt)) {
              ++foundCnt;
              break;
            }
          }
        }
        if ((*qIt)->followpos.size() == foundCnt) {
          SinQ = true;
          break;
        }
      }

      if (!SinQ) {
        Q.push_back(std::make_shared<PositionNode>(S));
        res.create_state(S.getFollowPosReadable(), false);
        log("New state: " + S.getFollowPosReadable());
      } else {
        log("State already in Q: " + S.getFollowPosReadable());
      }

      res.set_trans(R->getFollowPosReadable(), c, S.getFollowPosReadable());
      log("Set trans: " + R->getFollowPosReadable() + " --" + c + "--> " +
          S.getFollowPosReadable());
    });

    R = getNotMarked(Q, marked);

    ++cycle;
  }

  std::for_each(Q.begin(), Q.end(), [&res](std::shared_ptr<PositionNode> node) {
    for (auto it = node->followpos.begin(); it != node->followpos.end(); ++it) {
      auto position = dynamic_cast<PositionNode *>(it->get());
      if (position->name == SYMBOL_NUMBER_SIGN) {
        res.make_final(node->getFollowPosReadable());
        log("Set final: " + node->getFollowPosReadable());
        break;
      }
    }
  });

  return res;
}
