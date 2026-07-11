#include <iostream>

#include "mercury/version.hpp"

int main() {
    std::cout << mercury::kProjectName << " v" << mercury::version_string() << '\n';
    std::cout << "Exchange simulator — core engine scaffold ready.\n";
    return 0;
}
