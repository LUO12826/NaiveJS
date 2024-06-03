#ifndef NJS_TIMER_H
#define NJS_TIMER_H

#include <chrono>
#include <iostream>
#include <iomanip>

namespace njs {

constexpr int s_to_milli = 1000;
constexpr int milli_to_micro = 1000;
constexpr int s_to_micro = s_to_milli * milli_to_micro;

class Timer {

using TimePoint = std::chrono::steady_clock::time_point;

 public:
  explicit Timer(std::string name): name(std::move(name)) {
    start_time = std::chrono::steady_clock::now();
  }

  long long end(bool print_res = true) {
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - start_time);
    auto microsec_cnt = duration.count();

    if (print_res) {
      std::cout << "\033[33m"; // yellow text
      std::cout << "Timer: \"" << name << "\" in ";
      if (microsec_cnt / s_to_micro >= 1) {
        std::cout << std::setprecision(3) << (double)microsec_cnt / s_to_micro << " s.\n";
      } else {
        std::cout << std::setprecision(3) << (double)microsec_cnt / milli_to_micro << " ms.\n";
      }
      std::cout << "\033[0m";  // restore normal color
    }
    return microsec_cnt;
  }

 private:
  std::string name;
  TimePoint start_time;

};


}  // namespace njs



#endif  // NJS_TIMER_H