#include "api.hpp"
#include <string>

std::string dfa2re(DFA &d) {
  return d.to_string();
}
