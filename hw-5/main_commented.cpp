#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define EPSILON '_'
const std::string EPSILON_STR{EPSILON};

// std::string set_to_canonical_string(const auto &set_of_chars,
//                                     char special_symbol) {
//   std::string result;
//   bool has_special_symbol = false;
//   for (char c : set_of_chars) {
//     if (is_terminal(c))
//       result += c;
//     else
//       has_special_symbol = true;
//   }
//   std::sort(result.begin(), result.end());
//   if (has_special_symbol)
//     result += special_symbol;
//   return result;
// }

class CFGrammar {

  using StorageType = std::unordered_map<char, std::set<std::string>>;
  using RetType = std::unordered_map<char, std::set<char>>;

private:
  auto first_of_str(RetType &first, const std::string &Beta) {
    std::set<char> out;
    if (Beta == EPSILON_STR) {
      out.insert(EPSILON);
      return out;
    }
    bool nullable = true;
    for (char x : Beta) {
      if (is_terminal(x)) {
        out.insert(x);
        nullable = false;
        break;
      }
      for (char f : first[x])
        if (f != EPSILON)
          out.insert(f);
      if (!first.at(x).count(EPSILON)) {
        nullable = false;
        break;
      }
    }
    if (nullable)
      out.insert(EPSILON);
    return out;
  };

  bool is_terminal(char c) { return c >= 'a' && c <= 'z'; }
  bool is_non_terminal(char c) { return c >= 'A' && c <= 'Z'; }

public:
  void add_rule(char c, const std::string &s) {
    assert(is_non_terminal(c));
    if (s.empty()) {
      rules[c].insert(EPSILON_STR);
    } else {
      rules[c].insert(s);
    }
    non_terminals.insert(c);
    for (char ch : s) {
      if (is_terminal(ch))
        terminals.insert(ch);
      else
        non_terminals.insert(ch);
    }
  }

  void print_grammar() {
    for (auto &&[X, P] : rules) {
      for (auto &&Y : P) {
        std::cout << X << " -> " << Y << std::endl;
      }
    }
  }

  RetType get_first() {
    RetType first;

    for (auto &&[X, P] : rules) {
      for (auto &&rule : P) {
        if (rule == EPSILON_STR) {
          first[X].insert(EPSILON);
        }
      }
    }

    bool need_continue;
    do {
      need_continue = false;
      for (auto &&[X, P] : rules) {
        for (auto &&Y : P) {
          if (Y == EPSILON_STR)
            continue;
          bool all_nullable = true;
          for (char c : Y) {
            if (is_terminal(c)) {
              need_continue |= first[X].insert(c).second;
              all_nullable = false;
              break;
            }
            for (char f : first[c])
              if (f != EPSILON)
                need_continue |= first[X].insert(f).second;
            if (!first[c].count(EPSILON)) {
              all_nullable = false;
              break;
            }
          }
          if (all_nullable)
            need_continue |= first[X].insert(EPSILON).second;
        }
      }
    } while (need_continue);
    return first;
  }

  RetType get_follow() {
    RetType first = get_first();
    RetType follow{};

    follow[start].insert(end);

    for (auto &[A, prods] : rules)
      for (auto &p : prods)
        for (size_t i = 0; i < p.size(); ++i)
          if (is_non_terminal(p[i])) {
            auto fs = first_of_str(first, p.substr(i + 1));
            for (char t : fs)
              if (t != EPSILON)
                follow[p[i]].insert(t);
          }

    bool need_continue;
    do {
      need_continue = false;
      for (auto &[X, P] : rules) {
        for (auto &rule : P) {
          for (size_t i = 0; i < rule.size(); ++i) {
            char B = rule[i];
            if (!is_non_terminal(B))
              continue;

            std::string Beta = rule.substr(i + 1);
            auto fs = first_of_str(first, Beta);

            if (Beta.empty() || fs.count(EPSILON))
              for (char x : follow[X]) {
                if (x != EPSILON)
                  need_continue |= follow[B].insert(x).second;
              }
          }
        }
      }
    } while (need_continue);

    return follow;
  }

  bool is_ll1() {
    RetType first = get_first();
    RetType follow = get_follow();

    std::unordered_map<char, std::unordered_map<char, std::string>> M;

    for (auto &[X, P] : rules) {
      for (auto &rule : P) {
        auto F = first_of_str(first, rule);

        for (char a : F)
          if (a != EPSILON) {
            if (!M[X][a].empty() && M[X][a] != rule)
              return false;
            M[X][a] = rule;
          }

        if (F.count(EPSILON)) {
          for (char b : follow[X]) {
            if (!M[X][b].empty() && M[X][b] != rule)
              return false;
            M[X][b] = rule;
          }
        }
      }
    }

    return true;
  }

private:
  const char start = 'S';
  const char end = '$';
  std::unordered_set<char> terminals = {};
  std::unordered_set<char> non_terminals = {};
  StorageType rules = {};
};

// int main() {
//   std::string line;
//   std::regex rule_regex("^([A-Z])\\s*->\\s*([a-zA-Z]*)$");

//   CFGrammar grammar;
//   while (std::getline(std::cin, line)) {
//     std::smatch m;
//     if (std::regex_match(line, m, rule_regex)) {
//       grammar.add_rule(m[1].first[0], m[2]);
//     } else {
//       break;
//     }
//   }

//   // grammar.print_grammar();

//   std::vector<std::pair<char, std::string>> sorted_first;
//   for (auto &entry : grammar.get_first())
//     sorted_first.emplace_back(entry.first,
//                               set_to_canonical_string(entry.second, '_'));
//   std::sort(sorted_first.begin(), sorted_first.end());

//   std::cout << "First:" << std::endl;
//   for (auto &entry : sorted_first)
//     std::cout << entry.first << ": " << entry.second << std::endl;

//   std::vector<std::pair<char, std::string>> sorted_follow;
//   for (auto &entry : grammar.get_follow())
//     sorted_follow.emplace_back(entry.first,
//                                set_to_canonical_string(entry.second, '$'));
//   std::sort(sorted_follow.begin(), sorted_follow.end());

//   std::cout << "Follow:" << std::endl;
//   for (auto &entry : sorted_follow)
//     std::cout << entry.first << ": " << entry.second << std::endl;

//   std::cout << "is LL(1): " << (grammar.is_ll1() ? "Yes" : "No") << std::endl;
//   return 0;
// }