#ifndef NJS_TIMER_H
#define NJS_TIMER_H

#include <chrono>
#include <iostream>

namespace njs {

class Timer {

using TimePoint = std::chrono::steady_clock::time_point;

 public:
  explicit Timer(std::string name): name(std::move(name)) {
    start_time = std::chrono::steady_clock::now();
  }

  long long end(bool print_res = true) {
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - start_time);

    if (print_res) {
      std::cout << "\033[33m"; // yellow text
      std::cout << "Timer: \"" << name << "\" in " << duration.count() << " microseconds.\n";
      std::cout << "\033[0m";  // restore normal color
    }
    return duration.count();
  }

 private:
  std::string name;
  TimePoint start_time;

};


}  // namespace njs



#endif  // NJS_TIMER_H