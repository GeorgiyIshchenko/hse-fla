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

struct Rule {
  char lhs;
  std::string rhs;
};

struct Item {
  char lhs;
  std::string rhs;
  std::uint8_t dot;
  std::uint16_t start;

  bool is_complete() const { return dot == rhs.size(); }
  char next_symbol() const { return rhs[dot]; }
  bool operator==(const Item &o) const noexcept {
    return lhs == o.lhs && rhs == o.rhs && dot == o.dot && start == o.start;
  }
};

bool is_non_terminal(char ch) { return 'A' <= ch && ch <= 'Z'; }

struct ItemHash {
  std::size_t operator()(const Item &it) const noexcept {
    std::size_t h = std::hash<char>{}(it.lhs);
    h = h * 31 + std::hash<std::string>{}(it.rhs);
    h = h * 31 + std::hash<std::uint8_t>{}(it.dot);
    h = h * 31 + std::hash<std::uint16_t>{}(it.start);
    return h;
  }
};

class StateSet {
public:
  bool add(const Item &it) {
    auto [_, inserted] = lookup.insert(it);
    if (inserted)
      items.push_back(it);
    return inserted;
  }

  std::size_t size() const { return items.size(); }
  const Item &operator[](std::size_t i) const { return items[i]; }

  auto begin() { return items.begin(); }
  auto end() { return items.end(); }
  auto begin() const { return items.begin(); }
  auto end() const { return items.end(); }
  auto cbegin() const { return items.cbegin(); }
  auto cend() const { return items.cend(); }

private:
  std::unordered_set<Item, ItemHash> lookup;
  std::vector<Item> items;
};

int main() {
  std::ifstream in{"cyk.in", std::ios::in};
  std::ofstream out{"cyk.out", std::ios::out};

  std::string word{};
  in >> word;
  if (word == "_") {
    word.clear();
  }

  std::vector<Rule> grammar{};
  std::array<std::vector<std::size_t>, 26> rules_by_nt;

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty())
      continue;
    char lhs = line[0];
    std::string rhs = line.substr(2);
    if (rhs == "_") {
      rhs.clear();
    }
    std::size_t id = grammar.size();
    grammar.push_back({lhs, rhs});
    rules_by_nt[lhs - 'A'].push_back(id);
  }

  const std::size_t n = word.size();
  std::vector<StateSet> chart(n + 1);

  for (std::size_t id : rules_by_nt['S' - 'A']) {
    const Rule &r = grammar[id];
    chart[0].add({r.lhs, r.rhs, 0, 0});
  }

  for (std::size_t i = 0; i <= n; ++i) {
    for (std::size_t p = 0; p < chart[i].size(); ++p) {
      const Item item = chart[i][p];

      if (item.is_complete()) {
        StateSet &target = chart[i];
        StateSet &origin = chart[item.start];

        for (std::size_t k = 0; k < origin.size(); ++k) {
          const Item &prev = origin[k];
          if (!prev.is_complete() && prev.next_symbol() == item.lhs) {
            Item next = prev;
            ++next.dot;
            target.add(next);
          }
        }
      } else {
        char sym = item.next_symbol();
        if (is_non_terminal(sym)) {
          for (std::size_t id : rules_by_nt[sym - 'A']) {
            const Rule &r = grammar[id];
            chart[i].add({r.lhs, r.rhs, 0, static_cast<std::uint16_t>(i)});
          }
        } else {
          if (i < n && sym == word[i]) {
            Item next = item;
            ++next.dot;
            chart[i + 1].add(next);
          }
        }
      }
    }
  }

  bool accepted = false;
  for (const Item &it : chart[n]) {
    if (it.lhs == 'S' && it.is_complete() && it.start == 0) {
      accepted = true;
      break;
    }
  }

  out << (accepted ? "YES" : "NO") << std::endl;

  return 0;
}
