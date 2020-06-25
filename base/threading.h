#ifndef BASE_THREADING_H_
#define BASE_THREADING_H_

#include <memory>
#include <pthread.h>
#include <unistd.h>

#include "check.h"

namespace base {

// TODO(domfarolino): These should probably be factored out somewhere else.
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

} // namespace helper

template <typename... Ts>
class ThreadArg {
public:
  template <typename F>
  ThreadArg(F&& func, Ts&&... args)
      : f(std::forward<F>(func)),
        args(std::make_tuple(std::forward<Ts>(args)...)) {}

  std::function<void (Ts...)> f;
  std::tuple<Ts...> args;
};

class Thread {
public:
  template <typename F, typename... Ts>
  Thread(F&& func, Ts&&... args) {
    pthread_attr_init(&attributes_);
    pthread_create(&id_, &attributes_, start<Ts...>,
                   new ThreadArg<Ts...>(std::forward<F>(func),
                                        std::forward<Ts>(args)...));
  }

  static void sleep_for(std::chrono::milliseconds ms) {
    usleep(ms.count() * 1000);
  }

  void join() {
    pthread_join(id_, nullptr);
  }

  template <typename... Ts>
  static void* start(void* in) {
    std::unique_ptr<ThreadArg<Ts...>> thread_arg((ThreadArg<Ts...>*)in);
    CHECK(thread_arg);

    helper::invoker(thread_arg->f, thread_arg->args);
    pthread_exit(0);
  }

private:
  pthread_t id_;
  pthread_attr_t attributes_;
};

} // namespace base

#endif // BASE_THREADING_H_
