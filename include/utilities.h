#pragma once

#include <chrono>
#include <thread>


inline void sleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// We can add more utility helpers here if needed and dependant of the problem we are solving.
