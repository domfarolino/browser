#ifndef BASE_HELPER_H_
#define BASE_HELPER_H_

#include <tuple>

// Source: https://stackoverflow.com/questions/16868129
namespace helper {

template <std::size_t... Ts>
struct index {};

template <std::size_t N, std::size_t... Ts>
struct gen_seq : gen_seq<N - 1, N - 1, Ts...> {};

template <std::size_t... Ts>
struct gen_seq<0, Ts...> : index<Ts...> {};

template <typename F, typename... Args, std::size_t... Is>
void invoker(const F& function,
             std::tuple<Args...>& tup, helper::index<Is...>) {
  function(std::get<Is>(tup)...);
}

template <typename F, typename... Args>
void invoker(const F& function, std::tuple<Args...>& tup) {
  invoker(function, tup, helper::gen_seq<sizeof...(Args)>{});
}

}  // namespace helper

#endif  // BASE_HELPER_H_
