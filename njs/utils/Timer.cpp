#include "Timer.h"

#include <chrono>

namespace njs {

Timer::Timer(std::string name): name(std::move(name)) {
  start_time = std::chrono::steady_clock::now();
}

long long Timer::end(bool print_res) {
  auto endTime = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - start_time);

  if (print_res) {
    std::cout << "\033[32m"; // green text
    std::cout << "Timer: \"" << name << "\" in " << duration.count() << " microseconds." << std::endl;
    std::cout << "\033[0m";  // restore normal color
  }
  return duration.count();
}


}