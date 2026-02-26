//Standard C++ 11 time measure, platform independent

#include <iostream>
#include <chrono>

void my_function() {
    volatile int sum = 0;
    for (int i = 0; i < 10000000; ++i) {
        sum += i;
    }
}

void test_time_measure()
{
    auto start = std::chrono::steady_clock::now();

    my_function();

    auto end = std::chrono::steady_clock::now();

    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << "elapsed time: " << elapsed_seconds.count() << "sec" << std::endl;
}