#include "api.hpp"
#include <cstddef>
#include <functional>
#include <iostream>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// В рамках решения данной задачи я переписал псевдокод отсюда.
// https://neerc.ifmo.ru/wiki/index.php?title=%D0%9C%D0%B8%D0%BD%D0%B8%D0%BC%D0%B8%D0%B7%D0%B0%D1%86%D0%B8%D1%8F_%D0%94%D0%9A%D0%90,_%D0%B0%D0%BB%D0%B3%D0%BE%D1%80%D0%B8%D1%82%D0%BC_%D0%B7%D0%B0_O(n%5E2)_%D1%81_%D0%BF%D0%BE%D1%81%D1%82%D1%80%D0%BE%D0%B5%D0%BD%D0%B8%D0%B5%D0%BC_%D0%BF%D0%B0%D1%80_%D1%80%D0%B0%D0%B7%D0%BB%D0%B8%D1%87%D0%B8%D0%BC%D1%8B%D1%85_%D1%81%D0%BE%D1%81%D1%82%D0%BE%D1%8F%D0%BD%D0%B8%D0%B9
// Эта страница последний раз была отредактирована 4 сентября 2022 в 19:41.

std::unordered_set<std::string> accessible_from_start;

void dfs_accessible_from_start(std::string state, DFA &d) {
  accessible_from_start.insert(state);
  for (auto &&symbol : d.get_alphabet()) {
    if (d.has_trans(state, symbol)) {
      std::string toState = d.get_trans(state, symbol);
      if (toState != state &&
          accessible_from_start.find(toState) == accessible_from_start.end()) {
        dfs_accessible_from_start(toState, d);
      }
    }
  }
}

void remove_non_accessible(DFA &d) {
  dfs_accessible_from_start(d.get_initial_state(), d);
  std::vector<std::string> toDelete{};
  for (auto &&state : d.get_states()) {
    if (accessible_from_start.find(state) == accessible_from_start.end()) {
      toDelete.push_back(state);
    }
  }
  for (auto &&state : toDelete) {
    d.delete_state(state);
  }
}

DFA dfa_minim(DFA &d) {

  if (d.is_empty()) {
    return d;
  }

  std::vector<int> alphabet(d.get_alphabet().size(), 0);
  std::unordered_map<char, int> symbol_to_idx;
  {
    int idx = 0;
    for (auto &&symbol : d.get_alphabet()) {
      alphabet[idx] = symbol;
      symbol_to_idx[symbol] = idx;
      idx++;
    }
  }
  int alphabet_size = alphabet.size();

  remove_non_accessible(d);

  size_t n = d.get_states().size() + 1;

  std::unordered_map<std::string, int> index_of;
  index_of.reserve(n);
  std::vector<int> states_vec(n);
  int initial_idx = 0;
  std::vector<int> is_final(n, false);
  std::vector<std::vector<int>> trans(
      n, std::vector<int>(d.get_alphabet().size(), 0));
  std::vector<std::vector<std::vector<int>>> trans_back(
      n, std::vector<std::vector<int>>(d.get_alphabet().size(),
                                       std::vector<int>()));

  {
    int idx = 1;
    for (auto &&state : d.get_states()) {
      if (state == d.get_initial_state()) {
        initial_idx = idx;
      }
      states_vec[idx] = idx;
      is_final[idx] = d.is_final(state);
      index_of[state] = idx;
      idx++;
    }
  }

  {
    int idx = 1;
    for (auto &&state : d.get_states()) {
      for (auto &&symbol : d.get_alphabet()) {
        if (d.has_trans(state, symbol)) {
          trans[idx][symbol_to_idx[symbol]] =
              states_vec[index_of[d.get_trans(state, symbol)]];
          trans_back[index_of[d.get_trans(state, symbol)]]
                    [symbol_to_idx[symbol]]
                        .push_back(idx);
        } else {
          trans[idx][symbol_to_idx[symbol]] = 0;
          trans_back[0][symbol_to_idx[symbol]].push_back(idx);
        }
      }
      idx++;
    }
  }

  for (auto &&symbol : d.get_alphabet()) {
    trans_back[0][symbol_to_idx[symbol]].push_back(0);
  }

#ifdef DEBUG
  std::cout << "trans: " << std::endl;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < alphabet_size; j++) {
      std::cout << i << " " << std::string(1, alphabet[j]) << " " << trans[i][j]
                << " " << std::endl;
    }
    std::cout << "----" << std::endl;
  }
  std::cout << "trans_back: " << std::endl;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < alphabet_size; j++) {
      for (int k = 0; k < trans_back[i][j].size(); k++) {
        std::cout << i << " " << std::string(1, alphabet[j]) << " "
                  << trans_back[i][j][k] << std::endl;
      }
      std::cout << "++++" << std::endl;
    }
    std::cout << "----" << std::endl;
  }
#endif

  std::vector<std::vector<bool>> marked(n, std::vector<bool>(n, false));
  std::queue<std::pair<int, int>> q;

  // 3
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      if (!marked[i][j] && is_final[i] != is_final[j]) {
        marked[i][j] = true;
        marked[j][i] = true;
        q.push({i, j});
      }
    }
  }

  // 4
  while (!q.empty()) {
    auto [u, v] = q.front();
    q.pop();
    for (int c = 0; c < alphabet_size; c++) {
#ifdef DEBUG
      std::cout << u << " " << v << " " << c << " " << trans_back[u][c].size()
                << " " << trans_back[v][c].size() << std::endl;
#endif
      for (int rIdx = 0; rIdx < trans_back[u][c].size(); rIdx++) {
        for (int sIdx = 0; sIdx < trans_back[v][c].size(); sIdx++) {
          int r = trans_back[u][c][rIdx];
          int s = trans_back[v][c][sIdx];
#ifdef DEBUG
          std::cout << "r: " << r << " s: " << s << std::endl;
#endif
          if (!marked[r][s]) {
            marked[r][s] = true;
            marked[s][r] = true;
            q.push({r, s});
          }
        }
      }
    }
  }

#ifdef DEBUG
  std::cout << "----" << std::endl;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      std::cout << marked[i][j] << " ";
    }
    std::cout << std::endl;
  }
#endif

  std::vector<int> component(n, -1);

  for (int i = 0; i < n; i++) {
    if (!marked[0][i]) {
      component[i] = 0;
    }
  }

  DFA result(d.get_alphabet());

  int cnt = 1;
  for (int i = 1; i < n; i++) {
    if (component[i] == -1) {
      component[i] = cnt;
      result.create_state(std::to_string(cnt));
      if (i == initial_idx) {
        result.set_initial(std::to_string(cnt));
      }
      if (is_final[i]) {
        result.make_final(std::to_string(cnt));
      }
      for (int j = i + 1; j < n; j++) {
        if (!marked[i][j]) {
          component[j] = cnt;
          if (j == initial_idx) {
            result.set_initial(std::to_string(cnt));
          }
          if (is_final[j]) {
            result.make_final(std::to_string(cnt));
          }
        }
      }
      cnt++;
    }
  }

  for (int i = 0; i < n; i++) {
    for (int j = 0; j < trans[i].size(); j++) {
      result.set_trans(std::to_string(component[i]), alphabet[j],
                       std::to_string(component[trans[i][j]]));
    }
  }

  result.delete_state(std::to_string(0));

#ifdef DEBUG
  std::cout << "components: " << std::endl;
  for (int i = 0; i < n; i++) {
    std::cout << component[i] << " ";
  }
  std::cout << std::endl;
#endif

  return result;
}