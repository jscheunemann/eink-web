#pragma once
// Browser build: C++20 library pieces missing from Emscripten 3.1.51's libc++.
// Force-included into every C++ translation unit.
#ifdef __cplusplus
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <string>

namespace std {

#if !defined(__cpp_lib_atomic_ref)
// Single-threaded build (no pthreads): plain access is atomic by definition.
template <class T>
struct atomic_ref {
  T &ref;
  explicit atomic_ref(T &obj) : ref(obj) {}
  T load(memory_order = memory_order_seq_cst) const { return ref; }
  void store(T v, memory_order = memory_order_seq_cst) const { ref = v; }
  T exchange(T v, memory_order = memory_order_seq_cst) const {
    T old = ref;
    ref = v;
    return old;
  }
  T fetch_add(T v, memory_order = memory_order_seq_cst) const {
    T old = ref;
    ref = old + v;
    return old;
  }
  T fetch_sub(T v, memory_order = memory_order_seq_cst) const {
    T old = ref;
    ref = old - v;
    return old;
  }
  T operator=(T v) const {
    ref = v;
    return v;
  }
  T operator++() const { return ++ref; }
  T operator++(int) const { return ref++; }
  T operator--() const { return --ref; }
  T operator--(int) const { return ref--; }
  operator T() const { return ref; }
};
#endif

// Floating-point from_chars (libc++ 17 only has integral overloads).
// Locale-independent enough for CSS numbers, which never use a comma.
inline from_chars_result picco_web_from_chars_fp(const char *first, const char *last, double &value) {
  std::string buf(first, last);
  char *endp = nullptr;
  errno = 0;
  const double v = std::strtod(buf.c_str(), &endp);
  const size_t used = static_cast<size_t>(endp - buf.c_str());
  if (used == 0) return {first, errc::invalid_argument};
  if (errno == ERANGE) return {first + used, errc::result_out_of_range};
  value = v;
  return {first + used, errc{}};
}
inline from_chars_result from_chars(const char *first, const char *last, double &value,
                                    chars_format = chars_format::general) {
  return picco_web_from_chars_fp(first, last, value);
}
inline from_chars_result from_chars(const char *first, const char *last, float &value,
                                    chars_format = chars_format::general) {
  double d = 0;
  const auto r = picco_web_from_chars_fp(first, last, d);
  if (r.ec == errc{}) value = static_cast<float>(d);
  return r;
}

}  // namespace std
#endif
