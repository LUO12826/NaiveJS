#ifndef NJS_TIMER_H
#define NJS_TIMER_H

#include <chrono>
#include <iostream>

namespace njs {

class Timer {

using TimePoint = std::chrono::steady_clock::time_point;

 public:

  explicit Timer(std::string name);

  long long end(bool print_res = true);

 private:
  std::string name;
  TimePoint start_time;

};


}  // namespace njs



#endif  // NJS_TIMER_H