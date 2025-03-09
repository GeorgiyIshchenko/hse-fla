#include "api.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

void log(const std::string &str) {
#ifdef DEBUG
  std::cout << str << std::endl;
#endif
}

using TransType = std::string;
using StateType = std::string;

// state, state, trans
std::map<StateType, std::map<StateType, TransType>>
    toStateMap{}; // the first argument is a start
std::map<StateType, std::map<StateType, TransType>>
    fromStateMap{}; // the first argument is a finish

StateType deadState = "ispras";
TransType deadTrans = "";

void initialize_regex_transitions(DFA &dfa) {
  for (auto &&state : dfa.get_states()) {
    for (auto &&symbol : dfa.get_alphabet()) {
      if (dfa.has_trans(state, symbol)) {
        auto &&to_state = dfa.get_trans(state, symbol);
        if (toStateMap[state][to_state].empty()) {
          toStateMap[state][to_state] = std::string(1, symbol);
        } else {
          toStateMap[state][to_state] += "|" + std::string(1, symbol);
        }
        log("toStateMap From: " + state + " To: " + to_state +
            " Transition: " + std::string(1, symbol));
        if (fromStateMap[to_state][state].empty()) {
          fromStateMap[to_state][state] = std::string(1, symbol);
        } else {
          fromStateMap[to_state][state] += "|" + std::string(1, symbol);
        }
      }
    }
    if (dfa.is_final(state)) {
      toStateMap[state].insert({deadState, deadTrans});
      fromStateMap[deadState].insert({state, deadTrans});
    }
  }
}

struct CycledClosure {
  std::string state;
  std::string R;
};

std::vector<CycledClosure> check_self_cycled(const std::string &state) {
  std::vector<CycledClosure> result{};
  for (auto &&it = toStateMap[state].begin(); it != toStateMap[state].end();
       ++it) {
    if (it->first == state) {
      result.push_back(CycledClosure{state, it->second});
    }
  }
  return result;
}

// type: (initial, final, cycled, double cycled), number of inputs and outputs,
// state
using PrioritizedState = std::tuple<int, int, StateType>;

// In the head there are non-final, non-closured
std::priority_queue<PrioritizedState> pq{};

void push_state(const StateType &state, const DFA &dfa) {
  log("State: " + state + " initial: " + std::to_string(dfa.is_initial(state)) +
      " count: " +
      std::to_string(std::max(1ul, toStateMap[state].size()) *
                     std::max(1ul, fromStateMap[state].size())));
  pq.push({!dfa.is_initial(state),
           std::max(1ul, toStateMap[state].size()) *
               std::max(1ul, fromStateMap[state].size()),
           state});
}

void init_pq(DFA &dfa) {
  for (auto &&state : dfa.get_states()) {
    push_state(state, dfa);
  }
}

void printTrans() {
  for (auto &&it = toStateMap.begin(); it != toStateMap.end(); ++it) {
    for (auto &&it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
      log("From: " + it->first + " Transition: " + it2->second +
          " To: " + it2->first);
    }
  }
}

std::string make_parens(const TransType &trans) {
  return "(" + trans + ")";
};

TransType getSelfTransitionString(StateType state) {
  auto &&cycles = check_self_cycled(state);
  if (cycles.empty()) {
    return {};
  }
  TransType transition{};
  if (cycles.size()) {
    transition += "(";
    for (int i = 0; i < cycles.size(); ++i) {
      transition += i == cycles.size() - 1 ? cycles[i].R : cycles[i].R + "|";
    }
    transition += ")*";
  }
  log("New transition (from, trans, to): " + state + " -" + transition + "-> " +
      state);
  return transition;
}

void minimize_node(PrioritizedState &pState) {
  auto &&state = std::get<2>(pState);
  for (auto &&it1 = fromStateMap[state].begin();
       it1 != fromStateMap[state].end(); ++it1) {
    for (auto &&it2 = toStateMap[state].begin(); it2 != toStateMap[state].end();
         ++it2) {
      StateType from_state = it1->first;
      StateType to_state = it2->first;
      if (to_state == state || from_state == state) {
        continue;
      }
      TransType transition{};
      transition = make_parens(it1->second);
      auto &&cycles = check_self_cycled(state);
      if (cycles.size()) {
        transition += "(";
        for (int i = 0; i < cycles.size(); ++i) {
          transition += cycles[i].R;
          if (i != cycles.size() - 1) {
            transition += "|";
          }
        }
        transition += ")*";
      }
      transition += make_parens(it2->second);
      log("New transition (from, trans, to): " + from_state + " -" +
          transition + "-> " + to_state);

      // Adding new

      if (toStateMap[from_state].find(to_state) !=
          toStateMap[from_state].end()) {
        if (toStateMap[from_state][to_state] != transition) {
          toStateMap[from_state][to_state] =
              transition + "|" + toStateMap[from_state][to_state];
        }
      } else {
        toStateMap[from_state].insert({to_state, transition});
      }

      if (fromStateMap[to_state].find(from_state) !=
          fromStateMap[to_state].end()) {
        if (fromStateMap[to_state][from_state] != transition) {
          fromStateMap[to_state][from_state] += "|" + transition;
        }
      } else {
        fromStateMap[to_state].insert({from_state, transition});
      }

      // Erasing

      log("Erasing triple fromStateMap: " + state + " " +
          fromStateMap[to_state][state] + " " + to_state);
      fromStateMap[to_state].erase(state);
      log("Erasing triple toStateMap: " + from_state + " " +
          toStateMap[from_state][state] + " " + state);
      toStateMap[from_state].erase(state);
    }
  }
  log("Erasing state from maps: " + state);
  fromStateMap.erase(fromStateMap.find(state));
  toStateMap.erase(toStateMap.find(state));
  pq.pop();
  printTrans();
}

std::string dfa2re(DFA &d) {

  if (d.get_final_states().empty()) {
    return {};
  }

  log("Initialize regex transitions...");
  initialize_regex_transitions(d);

  log("Initialize priority queue...");
  init_pq(d);

  printTrans();

  if (!pq.size()) {
    return {};
  }

  PrioritizedState st = pq.top();
  while (std::get<0>(st) > 0) {
    log("Minimize node: " + std::get<2>(st));
    minimize_node(st);
    if (pq.empty()) {
      break;
    }
    st = pq.top();
  }

  printTrans();

  auto initial_cycled = getSelfTransitionString(d.get_initial_state());
  auto end = toStateMap[d.get_initial_state()][deadState];
  auto result = make_parens(make_parens(initial_cycled) + make_parens(end));

  return result;
}