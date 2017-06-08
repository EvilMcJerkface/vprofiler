#include <chrono>
#include <iostream>
#include <thread>

#include "test_src.h"
#include "trace_tool.h"

int main() {
    for (int i = 0; i < 100; ++i) {
        SESSION_START(std::to_string(i).c_str());
        int res = A();
        SESSION_END(true);
        std::cout << res << std::endl;
    }
    std::cout << "Sleep for 10s." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;
}

