#include <iostream>
#include <source_location>

constexpr int DEBUG = 1;

namespace spw_rmap::debug {

template <typename T>
void debug_impl(T&& msg, const std::source_location& loc =
                             std::source_location::current()) {
  std::cerr << loc.file_name() << " in line " << loc.line() << " in function "
            << loc.function_name() << ": " << std::forward<T>(msg) << '\n';
}

template <typename T>
constexpr void debug(T&& msg, const std::source_location& loc =
                                  std::source_location::current()) {
  if constexpr (DEBUG) {
    debug_impl(std::forward<T>(msg), loc);
  }
}

template <typename T, typename Arg>
void debug_impl(
    T&& msg, Arg&& value,
    const std::source_location& loc = std::source_location::current()) {
  std::cerr << loc.file_name() << " in line " << loc.line() << " in function "
            << loc.function_name() << ": " << std::forward<T>(msg)
            << std::forward<Arg>(value) << '\n';
}

template <typename T, typename Arg>
constexpr void debug(
    T&& msg, Arg&& value,
    const std::source_location& loc = std::source_location::current()) {
  if constexpr (DEBUG) {
    debug_impl(std::forward<T>(msg), std::forward<Arg>(value), loc);
  }
}

}  // namespace spw_rmap::debug
