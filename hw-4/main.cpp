// На первой строке входного файла записано входное слово в виде непустой
// последовательности символов длины не более 200, не содержащей пробельных
// символов и заглавных латинских букв.

// На каждой из последующих строк, количество которых не более 15, записано
// правило входной KC-грамматики в виде одной заглавной латинской буквы
// (названия нетерминала), пробела и правой части правила, записанной непустой
// последовательностью непробельных символов длиной не более 5, в указанном
// порядке.

// Начальный символ грамматики всегда именуется буквой S. Символ '_' во всём
// входе всегда означает пустое слово (последовательность из нуля символов).

// На выход требуется вывести одно слово: YES, если входное слово принадлежит
// языку входной грамматики, или NO в противном случае.

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// #define DEBUG

#ifdef DEBUG
using CharType = unsigned char;
#else
using CharType = uint16_t;
#endif

using ProdType = std::vector<CharType>;
using GrammarMap = std::unordered_map<CharType, std::vector<ProdType>>;
#ifdef DEBUG
using DynamicContainer =
    std::unordered_map<CharType, std::array<std::array<bool, 200>, 200>>;
#else
using DynamicContainer =
    std::unordered_map<CharType, std::array<std::bitset<200>, 200>>;
#endif

struct PairHasher {
  std::size_t operator()(const std::pair<CharType, CharType> p) const {
    return std::hash<CharType>()(p.first) + 0x9e3779b9 ^
           (std::hash<CharType>()(p.second) << 22);
  }
};

struct PairEqual {
  bool operator()(const std::pair<CharType, CharType> &lhs,
                  const std::pair<CharType, CharType> &rhs) const {
    return lhs.first == rhs.first && lhs.second == rhs.second;
  }
};

struct ProdPairHasher {
  std::size_t operator()(const std::pair<CharType, ProdType> &p) const {
    std::size_t hash = std::hash<CharType>()(p.first);
    for (const auto &ch : p.second) {
      hash ^=
          std::hash<CharType>()(ch) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    return hash;
  }
};

struct ProdPairEqual {
  bool operator()(const std::pair<CharType, ProdType> &lhs,
                  const std::pair<CharType, ProdType> &rhs) const {
    return lhs.first == rhs.first && lhs.second == rhs.second;
  }
};

template <typename T> struct VectorHasher {
  std::size_t operator()(const std::vector<T> &vec) const {
    std::size_t hash = 0;
    for (const auto &elem : vec) {
      hash ^= std::hash<T>()(elem) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    return hash;
  }
};

template <typename T> struct VectorEqual {
  bool operator()(const std::vector<T> &lhs, const std::vector<T> &rhs) const {
    if (lhs.size() != rhs.size())
      return false;
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
  }
};

CharType S = 'S';
#ifdef DEBUG
CharType new_S = static_cast<CharType>(200);
CharType old_S = static_cast<CharType>(0);
const CharType new_non_terminal_start = static_cast<CharType>(new_S + 1);
#else
CharType new_S = static_cast<CharType>(512);
CharType old_S = static_cast<CharType>(0);
const CharType new_non_terminal_start = static_cast<CharType>(new_S + 1);
#endif

inline bool is_non_terminal(CharType c) {
  return (c >= 'A' && c <= 'Z') || new_S <= c;
}
inline bool is_terminal(CharType c) { return !is_non_terminal(c); }

CharType new_non_terminal() {
  static CharType nxt = new_non_terminal_start;
  return nxt++;
}

struct Grammar {
  GrammarMap prods{};
  std::unordered_set<CharType> terminals{};
  std::unordered_set<CharType> non_terminals{};
  std::unordered_map<CharType, CharType> terminal_substitutes{};
  bool eps_producing = false;

  void delete_long_prods() {
    auto non_terminals_copy = non_terminals;
    for (auto &&A : non_terminals) {
      auto &rules = prods[A];
      for (size_t i = 0; i < rules.size(); ++i) {
        auto &rule = rules[i];
        if (rule.size() > 2) {
          CharType next_after_a1;
          for (size_t k = 0; k < rule.size() - 2; ++k) {
            CharType n = new_non_terminal();
            if (k == 0) {
              next_after_a1 = n;
            }
            non_terminals_copy.insert(n);
            prods[n] = {};
            if (k == rule.size() - 3) {
              prods[n].push_back({rule[k + 1], rule[k + 2]});
            } else {
              prods[n].push_back({rule[k + 1], static_cast<CharType>(n + 1)});
            }
          }
          rules[i] = {rule[0], next_after_a1};
        }
      }
    }
    non_terminals = non_terminals_copy;
  }

  void delete_epsilon_prods() {
    std::unordered_set<CharType> epsilons{};
    for (auto &&[A, rules] : prods) {
      for (auto &&rule : rules) {
        if (rule.empty()) {
          epsilons.insert(A);
        }
      }
    }

    bool changed = true;
    while (changed) {
      changed = false;
      for (auto &&[A, rules] : prods) {
        if (epsilons.count(A)) {
          continue;
        }
        for (auto &&rule : rules) {
          bool all_in_epsilons = true;
          for (auto &&ch : rule) {
            if (!epsilons.count(ch)) {
              all_in_epsilons = false;
            }
          }
          if (all_in_epsilons) {
            epsilons.insert(A);
            changed = true;
          }
        }
      }
    }

    auto prods_copy = prods;
    for (auto &&[A, rules] : prods) {
      for (auto &&rule : rules) {
        if (rule.size() != 2) {
          continue;
        }
        auto left = rule[0];
        auto right = rule[1];
        bool epsilon_left = epsilons.count(left);
        bool epsilon_right = epsilons.count(right);
        if (epsilon_left) {
          prods_copy[A].push_back({right});
        }
        if (epsilon_right) {
          prods_copy[A].push_back({left});
        }
        if (epsilon_left && epsilon_right) {
          prods_copy[A].push_back({});
        }
      }
    }
    prods = prods_copy;

    GrammarMap new_prods{};
    for (auto &&[A, rules] : prods) {
      for (auto &&rule : rules) {
        if (!rule.empty()) {
          if (new_prods.count(A) == 0) {
            new_prods[A] = {};
          }
          new_prods[A].push_back(rule);
        }
      }
    }
    prods = new_prods;

    if (epsilons.count(S)) {
      eps_producing = true;
      non_terminals.insert(new_S);
      prods[new_S] = {};
      prods[new_S].push_back({S});
      prods[new_S].push_back({});
      old_S = S;
      S = new_S;
    }
  }

  void delete_chains() {
    std::unordered_set<std::pair<CharType, CharType>, PairHasher, PairEqual>
        chains;
    for (auto &&A : non_terminals) {
      chains.insert({A, A});
    }
    bool changed = true;
    while (changed) {
      changed = false;
      auto chains_copy = chains;
      for (auto &&[A, B] : chains_copy) {
        if (prods.count(B) == 0) {
          continue;
        }
        for (auto &&rule : prods[B]) {
          if (rule.size() == 1 && is_non_terminal(rule[0])) {
            if (!chains.count({A, rule[0]})) {
              chains.insert({A, rule[0]});
              changed = true;
            }
          }
        }
      }
    }
    GrammarMap prods_copy = {};
    for (auto &&[A, B] : chains) {
      if (prods.count(B) == 0) {
        continue;
      }
      for (auto &&rule : prods[B]) {
        if (!(rule.size() == 1 && is_non_terminal(rule[0]))) {
          prods_copy[A].push_back(rule);
        }
      }
    }
    for (auto &&[A, rules] : prods) {
      for (auto &&rule : rules) {
        if (!(rule.size() == 1 && is_non_terminal(rule[0]))) {
          prods_copy[A].push_back(rule);
        }
      }
    }
    prods = prods_copy;
  }

  void delete_non_producing() {
    std::unordered_map<CharType, bool> is_producing{};

    for (auto &&A : non_terminals) {
      auto &rules = prods[A];
      for (auto &&rule : rules) {
        if (rule.empty() || (rule.size() == 1 && is_terminal(rule[0]))) {
          is_producing[A] = true;
        }
      }
    }

    bool changed = false;
    do {
      changed = false;
      for (auto &&[A, rules] : prods) {
        for (auto &&rule : rules) {
          bool all_useful = true;
          for (size_t i = 0; i < rule.size(); ++i) {
            if (is_terminal(rule[i])) {
              continue;
            }
            if (!is_producing[rule[i]]) {
              all_useful = false;
              break;
            }
          }
          if (all_useful && !is_producing[A]) {
            is_producing[A] = true;
            changed = true;
          }
        }
      }
    } while (changed);

    std::unordered_set<CharType> nt_cleared{};
    for (auto A : non_terminals) {
      if (!is_producing[A]) {
        prods.erase(A);
      } else {
        nt_cleared.insert(A);
      }
    }
    non_terminals = nt_cleared;
  };

  void delete_non_acessible() {
    std::set<CharType> accessible{};
    accessible.insert(S);
    bool changed = false;
    do {
      changed = false;
      for (auto &&[A, rules] : prods) {
        if (accessible.count(A)) {
          for (auto &&rule : rules) {
            for (auto ch : rule) {
              changed |= accessible.insert(ch).second;
            }
          }
        }
      }
    } while (changed);
    std::unordered_set<CharType> t_cleared{};
    for (auto &&ch : terminals) {
      if (accessible.count(ch)) {
        t_cleared.insert(ch);
      }
    }
    terminals = t_cleared;
    std::unordered_set<CharType> nt_cleared{};
    for (auto &&ch : non_terminals) {
      if (accessible.count(ch)) {
        nt_cleared.insert(ch);
      } else {
        prods.erase(ch);
      }
    }
    non_terminals = nt_cleared;
  }

  void delete_useless() {
    delete_non_producing();
    delete_non_acessible();
  }

  void new_prods_for_terminals() {

    for (auto &&[A, rules] : prods) {
      for (auto &&rule : rules) {
        if (rule.size() == 1 && is_terminal(rule[0])) {
          terminal_substitutes[rule[0]] = A;
        }
      }
    }

    auto prods_copy = prods;
    for (auto &&[A, rules] : prods) {
      for (size_t i = 0; i < rules.size(); ++i) {
        auto &rule = rules[i];
        if (rule.size() == 2 &&
            (is_terminal(rule[0]) || is_terminal(rule[1]))) {
          auto left = rule[0];
          auto right = rule[1];
          if (is_terminal(left)) {
            CharType new_nt;
            if (terminal_substitutes.count(left)) {
              new_nt = terminal_substitutes[left];
            } else {
              new_nt = new_non_terminal();
              terminal_substitutes[left] = new_nt;
            }
            non_terminals.insert(new_nt);
            if (!prods_copy.count(new_nt)) {
              prods_copy[new_nt] = {};
            }
            prods_copy[new_nt].push_back({left});
            left = new_nt;
          }
          if (is_terminal(right)) {
            CharType new_nt;
            if (terminal_substitutes.count(right)) {
              new_nt = terminal_substitutes[right];
            } else {
              new_nt = new_non_terminal();
              terminal_substitutes[right] = new_nt;
            }
            non_terminals.insert(new_nt);
            if (!prods_copy.count(new_nt)) {
              prods_copy[new_nt] = {};
            }
            prods_copy[new_nt].push_back({right});
            right = new_nt;
          }
          prods_copy[A][i] = {left, right};
        }
      }
    }
    prods = prods_copy;
  }

  void to_cnf() {
    print_grammar("Initial grammar:");
    delete_long_prods();
    print_grammar("After deleting long productions:");
    delete_epsilon_prods();
    print_grammar("After deleting epsilon productions:");
    delete_chains();
    print_grammar("After deleting chains:");
    delete_useless();
    print_grammar("After deleting useless:");
    new_prods_for_terminals();
    print_grammar("After adding new productions for terminals:");
  }

  void print_grammar(std::string prefix = "") {
    (void)prefix;
#ifdef DEBUG
    std::cout << prefix << std::endl;
    std::cout << "Terminals: ";
    for (auto &&ch : terminals) {
      std::cout << (u_char)ch << ' ';
    }
    std::cout << std::endl;
    std::cout << "Non-terminals: ";
    for (auto &&ch : non_terminals) {
      std::cout << (u_char)ch << ' ';
    }
    std::cout << std::endl;
    for (auto &&[X, P] : prods) {
      for (auto &&Y : P) {
        std::cout << (u_char)X << " -> ";
        for (auto &&ch : Y) {
          std::cout << (u_char)ch << ' ';
        }
        std::cout << std::endl;
      }
    }
#endif
  }
};

ProdType string_to_pt(std::string s) {
  ProdType pt{};
  for (auto &&ch : s) {
    pt.push_back(ch);
  }
  return pt;
}

void print_cyk(DynamicContainer &d) {
  (void)d;
#ifdef DEBUG
  std::cout << "CYK:" << std::endl;
  for (auto &&[A, rs] : d) {
    std::cout << (u_char)A << ": " << std::endl;
    for (auto &&r : d[A]) {
      for (auto &&ch : r) {
        std::cout << ch << ' ';
      }
      std::cout << std::endl;
    }
  }
#endif
}

bool cyk(Grammar &grammar, const std::string &s) {
  if (s == "_" && grammar.eps_producing) {
    return true;
  }

  std::unordered_set<CharType> non_terminals{};
  for (auto &&ch : grammar.non_terminals) {
    non_terminals.insert(ch);
  }

  std::unordered_map<CharType,
                     std::unordered_set<ProdType, VectorHasher<CharType>,
                                        VectorEqual<CharType>>>
      prods{};
  for (auto &&[A, rs] : grammar.prods) {
    for (auto &&r : rs) {
      prods[A].insert(r);
    }
  }
  size_t n = s.size();
  DynamicContainer d;

  for (auto &&A : non_terminals) {
    d[A] = {};
  }

  for (size_t i = 0; i < n; ++i) {
    for (const auto &[A, rs] : prods) {
      for (const auto &r : rs) {
        if (r.size() == 1 && is_terminal(r[0]) &&
            static_cast<char>(r[0]) == s[i]) {
          d[A][i][i] = true;
        }
      }
    }
  }

  for (size_t len = 2; len <= n; ++len) {
    for (size_t i = 0; i + len <= n; ++i) {
      size_t j = i + len - 1;
      for (const auto &[A, rs] : prods) {
        for (const auto &r : rs) {
          if (r.size() == 2) {
            CharType B = r[0];
            CharType C = r[1];
            if (!d.count(B) || !d.count(C)) {
              continue;
            }
            for (size_t k = i; k < j; ++k) {
              if (d[B][i][k] && d[C][k + 1][j]) {
                d[A][i][j] = true;
                break;
              }
            }
          }
        }
      }
    }
  }

  //print_cyk(d);

  return d.count(S) && d[S][0][n - 1];
}

int main() {

  std::ifstream in{"cyk.in", std::ios::in};

  std::string s{};
  in >> s;

  Grammar grammar{};
  GrammarMap prods{};

  std::string line;

  while (std::getline(in, line)) {
    if (line.empty())
      continue;
    char A{line[0]};
    grammar.non_terminals.insert(A);
    std::string rule{line.substr(2)};
    if (rule == "_") {
      prods[A].push_back({});
      continue;
    }
    ProdType rule_vec{};
    for (auto ch : rule) {
      rule_vec.push_back(ch);
      if (is_terminal(ch)) {
        grammar.terminals.insert(ch);
      } else {
        grammar.non_terminals.insert(ch);
      }
    }
    prods[A].push_back(rule_vec);
  }

  grammar.prods = prods;

  grammar.to_cnf();

  std::ofstream out{"cyk.out", std::ios::out};

  if (cyk(grammar, s))
    out << "YES" << std::endl;
  else
    out << "NO" << std::endl;

  return 0;
}
