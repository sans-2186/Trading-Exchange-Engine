#include <iostream>

#include "mercury/version.hpp"

int main() {
    std::cout << "Mercury benchmarks — stub (Google Benchmark added in Phase 4)\n";
    std::cout << "Version: " << mercury::version_string() << '\n';
    return 0;
}
