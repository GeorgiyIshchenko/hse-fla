#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

int generateReadableNum() {
  static int num = 1;
  return num++;
}

struct PositionNode;

struct BaseNode {

  BaseNode(Token token, bool nullable = false)
      : token(token), nullable(nullable) {}

  virtual ~BaseNode() {}

  Token token;
  bool nullable;
  std::unordered_set<std::shared_ptr<BaseNode>> firstpos{};
  std::unordered_set<std::shared_ptr<BaseNode>> lastpos{};
  std::set<int> followpos{};

  int readableNum{};
};

struct OrNode : BaseNode {

  OrNode(std::shared_ptr<BaseNode> left, std::shared_ptr<BaseNode> right)
      : BaseNode(Token(TokenType::TOK_OR, SYMBOL_OR), true) {
    this->left = left;
    this->right = right;
    nullable = left->nullable || right->nullable;
    std::for_each(
        left->firstpos.begin(), left->firstpos.end(),
        [this](std::shared_ptr<BaseNode> node) { firstpos.insert(node); });
    std::for_each(
        left->lastpos.begin(), left->lastpos.end(),
        [this](std::shared_ptr<BaseNode> node) { lastpos.insert(node); });
    std::for_each(
        right->firstpos.begin(), right->firstpos.end(),
        [this](std::shared_ptr<BaseNode> node) { firstpos.insert(node); });
    std::for_each(
        right->lastpos.begin(), right->lastpos.end(),
        [this](std::shared_ptr<BaseNode> node) { lastpos.insert(node); });
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
    std::for_each(
        repeatable->firstpos.begin(), repeatable->firstpos.end(),
        [this](std::shared_ptr<BaseNode> node) { firstpos.insert(node); });
    std::for_each(
        repeatable->lastpos.begin(), repeatable->lastpos.end(),
        [this](std::shared_ptr<BaseNode> node) { lastpos.insert(node); });
    std::for_each(repeatable->lastpos.begin(), repeatable->lastpos.end(),
                  [this, &repeatable](std::shared_ptr<BaseNode> node) {
                    std::for_each(repeatable->firstpos.begin(),
                                  repeatable->firstpos.end(),
                                  [this, &node](std::shared_ptr<BaseNode> n) {
                                    node->followpos.insert(n->readableNum);
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
                    firstpos.insert(node);
                  });
    std::for_each(left->lastpos.begin(), left->lastpos.end(),
                  [this, &right](std::shared_ptr<BaseNode> node) {
                    if (right->nullable) {
                      lastpos.insert(node);
                    }
                  });
    std::for_each(right->firstpos.begin(), right->firstpos.end(),
                  [this, &left](std::shared_ptr<BaseNode> node) {
                    if (left->nullable) {
                      firstpos.insert(node);
                    }
                  });
    std::for_each(
        right->lastpos.begin(), right->lastpos.end(),
        [this](std::shared_ptr<BaseNode> node) { lastpos.insert(node); });
    std::for_each(left->lastpos.begin(), left->lastpos.end(),
                  [this, &right](std::shared_ptr<BaseNode> node) {
                    std::for_each(
                        right->firstpos.begin(), right->firstpos.end(),
                        [this, &node](std::shared_ptr<BaseNode> n) {
                          log(std::to_string(node->readableNum) + " <-- " +
                              std::to_string(n->readableNum));
                          node->followpos.insert(n->readableNum);
                        });
                  });
  }

  virtual ~ConcatNode() {}

  std::shared_ptr<BaseNode> left;
  std::shared_ptr<BaseNode> right;
};

std::vector<std::shared_ptr<PositionNode>> globalPositions{};
std::unordered_map<char, std::unordered_set<int>> symbolToPositions{};
std::unordered_map<int, std::set<int>> globalFollowpos{};
int numberSign = 0;

struct PositionNode : BaseNode, std::enable_shared_from_this<PositionNode> {
  PositionNode(char name = SYMBOL_HELPER_POSITION, bool nullable = false)
      : BaseNode(Token(TokenType::NODE, name), nullable), name(name) {
    readableNum = generateReadableNum();
  }

  virtual ~PositionNode() {}

  void initializePositions() {
    auto self = shared_from_this();
    if (!nullable) {
      firstpos.insert(self);
      lastpos.insert(self);
    }
    if (name != SYMBOL_HELPER_POSITION) {
      if (symbolToPositions.find(name) == symbolToPositions.end()) {
        symbolToPositions[name] = {readableNum};
      } else {
        symbolToPositions[name].insert(readableNum);
      }
    }
    if (name == SYMBOL_NUMBER_SIGN) {
      numberSign = readableNum;
    }
    globalPositions.push_back(self);
  }

  char name;
  std::string conditionName{};
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
    input = "(" + input + ")#";
    for (int i = 0; i < input.size(); ++i) {
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
        result.push_back(Token(TokenType::NODE, SYMBOL_NUMBER_SIGN));
        break;
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
      log("Parse or");
    }
    return res;
  }

  std::shared_ptr<BaseNode> parseConcat() {
    auto res = parseRepeat();
    while (cursor != input.end() && cursor->type == TokenType::TOK_CONCAT) {
      ++cursor;
      res = std::make_shared<ConcatNode>(res, parseRepeat());
      log("Parse concat");
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
    // There is no self positions for EmptyNode
    auto res = std::make_shared<EmptyNode>();
    res->initializePositions();
    return res;
  }

  std::shared_ptr<BaseNode> parse() {
    resultRoot = parseOr();
    return resultRoot;
  }
};

std::string setToCondition(const std::set<int> &positions) {
  std::string conditionName = "";
  for (auto pos : positions) {
    try {
      conditionName += std::to_string(pos);
      conditionName += ";";
    } catch (...) {
      log("Can't cast to PositionNode");
    }
  }
  return conditionName.substr(0, conditionName.size() - 1);
}

uint hash(const std::set<int> &positions) {
  uint32_t res = 0;
  for (const auto& pos : positions) {
      res ^= std::hash<int>()(pos) + 0x9e3779b9 + (res << 6) + (res >> 2);
  }
  return res;
}

DFA re2dfa(const std::string &s) {

  if (s.empty()) {
    auto res = DFA(Alphabet("ispras"));
    res.create_state("ispras");
    res.set_initial("ispras");
    res.make_final("ispras");
    return res;
  }

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

  std::set<int> R{};
  std::for_each(
      root->firstpos.begin(), root->firstpos.end(),
      [&R](std::shared_ptr<BaseNode> node) { R.insert(node->readableNum); });

  std::priority_queue<std::pair<bool, std::set<int>>> Q{};
  std::unordered_set<uint> visited{};
  Q.push({true, R});
  res.create_state(setToCondition(R), false);
  visited.insert(hash(R));
  res.set_initial(setToCondition(R));

  log("\nFollowpos:");
  std::string folowpos_str{};
  for (auto it = globalPositions.begin(); it != globalPositions.end(); ++it) {
    globalFollowpos[(*it)->readableNum] = (*it)->followpos;
    folowpos_str += std::to_string((*it)->readableNum) + " | " +
                    setToCondition((*it)->followpos) + "\n";
  }
  log(folowpos_str);

  int cycle = 1;

  std::set<int> errorSet = {-1};
  while (Q.top().first == true) {

    log("\nCycle " + std::to_string(cycle) + ". R: " + setToCondition(R));

    Q.pop();

    std::for_each(alpabet.begin(), alpabet.end(), [&R, &Q, &res, &visited](char c) {
      std::set<int> S{};

      std::for_each(symbolToPositions[c].begin(), symbolToPositions[c].end(),
                    [&R, &S](int node) {
                      if (std::find(R.begin(), R.end(), node) != R.end()) {
                        for (auto it = globalFollowpos[node].begin();
                             it != globalFollowpos[node].end(); ++it) {
                          S.insert(*it);
                        }
                      }
                    });

      if (S.size() == 0) {
        return;
      }

      if (std::find(visited.begin(), visited.end(), hash(S)) == visited.end()) {
        Q.push({true, S});
        res.create_state(setToCondition(S), false);
        visited.insert(hash(S));
        log("New state: " + setToCondition(S));
      } else {
        log("State already in Q: " + setToCondition(S));
      }

      res.set_trans(setToCondition(R), c, setToCondition(S));
      log("Set trans: " + setToCondition(R) + " ]--" + c + "--> " +
          setToCondition(S));
    });

    Q.push({false, R});
    log("Marked state: " + setToCondition(R));
    R = Q.top().second;

    ++cycle;
  }

  while (!Q.empty()) {
    std::pair<bool, std::set<int>> p = Q.top();
    Q.pop();
    auto node = p.second;
    for (auto it = node.begin(); it != node.end(); ++it) {
      if (*it == numberSign) {
        res.make_final(setToCondition(node));
        log("Set final: " + setToCondition(node));
        break;
      }
    }
  }

  return res;
}