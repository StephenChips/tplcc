#ifndef TPLCC_HELPER_H
#define TPLCC_HELPER_H

template <class... Ts>
struct overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overload(Ts...) -> overload<Ts...>;  // clang needs this deduction guide,
                                     // even in C++20 for some reasons ...

#endif