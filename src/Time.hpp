﻿#pragma once

//
// ゲーム内時間管理
//

#include <chrono>


namespace ngs {

class Time {
  using Duration = std::chrono::duration<double>;
  
  std::chrono::time_point<std::chrono::steady_clock, Duration> begin_;



public:
  Time()
    : begin_(std::chrono::steady_clock::now())
  {}

  Time(const double value)
    : begin_(Duration(value))
  {}


  double getDuration() const {
    return begin_.time_since_epoch().count();
  }

  double operator-(const Time& rhs) const {
    return (begin_.time_since_epoch() - rhs.begin_.time_since_epoch()).count();
  }
  
};

}
