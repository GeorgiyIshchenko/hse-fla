clang++ -Wall -Wextra -std=c++23 -DDEBUG earley.cpp -o main
./main
# LLVM_PROFILE_FILE="main.profraw" | ./main
# llvm-profdata merge -output=main.profdata default.profraw
# llvm-profdata show main.profdata --counts --all-functions --verify-region-info > profiling.txt
