#pragma once

#include <memory>
#include <type_traits>
#include <functional>
#include <iostream>

template <class T, class D>
std::unique_ptr<T, D> make_unique_del(T* p, D&& deleter) {
  return std::unique_ptr<T, D>{p, std::forward<D>(deleter)};
}

template <typename F>
struct function_traits;

template <typename R, typename A>
struct function_traits<R (&)(A)> {
  using result_type = R;
  using arg_type    = A;
};

template <class F>
using ArgOf = typename std::remove_pointer<typename function_traits<F>::arg_type>::type;

template <class D>
std::unique_ptr<ArgOf<D>, D> make_unique_del(std::nullptr_t, D&& deleter) {
  return make_unique_del<ArgOf<D>, D>(nullptr, std::forward<D>(deleter));
}

class destroy_guard {
public:
  destroy_guard() {}

  destroy_guard(std::function<void()> f)
    : _f(f) {
  }

  virtual ~destroy_guard() {
    if (_f) {
      _f();
    }
  }

  destroy_guard(destroy_guard&& that) {
    std::swap(_f, that._f);
    that._f = nullptr;
  }

  destroy_guard& operator =(destroy_guard&& that) {
    if (this != &that) {
      std::swap(_f, that._f);
    }
    return *this;
  }

  void reset() {
    _f = nullptr;
  }

private:
  destroy_guard(destroy_guard const&) = delete;
  destroy_guard& operator=(destroy_guard const&) = delete;

  std::function<void()> _f;
};

std::string randomNumString();

inline bool isOdd(int x) { return x & 1; }
inline bool isEven(int x) { return !isOdd(x); }

void printErrWithSource(const std::runtime_error& e, const std::string source);
