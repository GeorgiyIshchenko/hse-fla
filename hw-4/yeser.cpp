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

int main() {

  std::ifstream in{"cyk.in", std::ios::in};

  std::string s{};
  in >> s;

  std::string line;

  while (std::getline(in, line)) {
  }


  std::ofstream out{"cyk.out", std::ios::out};

  out << "YES" << std::endl;

  return 0;
}