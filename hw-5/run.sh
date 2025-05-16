set -e
clang++ -Wextra -Wall -std=c++23 main.cpp
echo 'S -> ABC
A -> aA
A -> 
B -> b
B -> C
C -> c' | ./a.out