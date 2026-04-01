#include <bits/stdc++.h>
#include "MemoryRiver.hpp"

// The OJ may compile and run the produced binary without specific I/O requirements for this header task.
// Provide a minimal main that does nothing but ensures linkage.

int main() {
    // Basic smoke to ensure template instantiation compiles for a common type.
    MemoryRiver<int, 2> mr(".memriver.bin");
    // Do not perform file operations in judge environment if not needed.
    // The program is supposed to be a header/library submission, so exit immediately.
    return 0;
}

