#pragma once

#include <iostream>
#include <string_view>

namespace test_support {

inline int failures = 0;

inline void check(bool condition, std::string_view expression, int line) {
  if (!condition) {
    std::cerr << "line " << line << ": check failed: " << expression << '\n';
    ++failures;
  }
}

template <typename ExpectedException, typename Function>
void check_throws(Function&& function, std::string_view expression, int line) {
  try {
    function();
    std::cerr << "line " << line << ": expected exception: " << expression
              << '\n';
    ++failures;
  } catch (const ExpectedException&) {
    // Expected path.
  } catch (...) {
    std::cerr << "line " << line << ": wrong exception type: " << expression
              << '\n';
    ++failures;
  }
}

} // namespace test_support

#define CHECK(expression) \
  ::test_support::check(static_cast<bool>(expression), #expression, __LINE__)

#define CHECK_THROWS_AS(expression, exception_type)                            \
  ::test_support::check_throws<exception_type>(                                \
      [&] { static_cast<void>(expression); }, #expression, __LINE__)

