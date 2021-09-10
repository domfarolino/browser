#ifndef BASE_CALLBACK_H_
#define BASE_CALLBACK_H_

#include <assert.h>

#include <functional>
#include <memory>
#include <tuple>

// The code in this file is inspired from Chromium's //base implementation [1],
// countless Stack Overflow articles that helped me (Dom Farolino) understand
// some pretty complex language features, and lots of messages with Daniel Cheng
// (dcheng@chromium.org) that I'm very grateful for.
//
// [1]: https://source.chromium.org/chromium/chromium/src/+/main:base/bind.h

namespace base {

// `BindStateBase` only exists for type erasure -- that is, to allow `OnceClosure`
// to hold onto a `BindState` while being blind to its bound return type and
// argument types.
class BindStateBase {
 public:
  BindStateBase() = default;
  virtual ~BindStateBase() = default;
  virtual void Invoke() = 0;
};

template <typename Functor, typename... Args>
class BindState final : public BindStateBase {
  // Note that we always decay the types that go into `BindState` so that
  // `BindState` retains value semantics (i.e., doesn't hold onto implicit
  // references of any kind). See documentation in `BindOnce()` for more details
  // on what this entails.
  static_assert(!(std::is_rvalue_reference_v<Args> && ...));

 public:
  BindState() = delete;
  template <typename FwdFunctor,
            typename... FwdArgs,
            typename =
                std::enable_if_t<(std::is_convertible_v<FwdArgs&&, Args> && ...)>>
  BindState(FwdFunctor&& f, FwdArgs&&... args)
      : f_(std::forward<FwdFunctor>(f)), args_(std::forward<FwdArgs>(args)...) {}

  BindState(const BindState& other) = delete;
  BindState& operator=(const BindState& other) = delete;
  BindState(BindState&& other) = default;
  BindState& operator=(BindState&& other) = default;

  void Invoke() override {
    auto seq =
      std::make_index_sequence<std::tuple_size<decltype(args_)>::value>{};
    InvokeImpl(std::move(f_), std::move(args_), seq);
  }

  template <typename BoundFunctor, typename BoundArgs, size_t... Is>
  void InvokeImpl(BoundFunctor&& f, BoundArgs&& args,
                  std::index_sequence<Is...>) {
    if constexpr (std::is_member_function_pointer<BoundFunctor>::value) {
      std::mem_fn(std::forward<BoundFunctor>(f))
          (std::get<Is>(std::forward<BoundArgs>(args)) ...);

    } else {
      std::forward<BoundFunctor>(f)
          (std::get<Is>(std::forward<BoundArgs>(args)) ...);
    }
  }

 private:
  Functor f_;
  std::tuple<Args...> args_;
};

inline void InvokeLambda(std::function<void()> lambda) {
  lambda();
}

class OnceClosure;

template <typename Lambda>
OnceClosure BindOnce(Lambda&& lambda);

template <typename Functor, typename... Args>
OnceClosure BindOnce(Functor&& f, Args&&... args);

// The `OnceClosure` class is just a light wrapper around `BindStateBase`. Once
// we introduce a generic `Callback` class that supports partially-bound
// functors and consumers supplying unbound arguments at invocation time,
// `OnceClosure` can be a specialization of that class.
class OnceClosure {
 public:
  OnceClosure() = default;
  explicit OnceClosure(std::unique_ptr<BindStateBase> bind_state) :
      bind_state_(std::move(bind_state)) {}
  OnceClosure(const OnceClosure& other) = delete;
  OnceClosure& operator=(const OnceClosure& other) = delete;
  OnceClosure(OnceClosure&& other) = default;
  OnceClosure& operator=(OnceClosure&& other) = default;

  // Overload that delegates to `BindOnce()`. This is just so we can support
  // assigning a `OnceClosure` to a lambda.
  template <typename Lambda>
  OnceClosure(Lambda&& lambda) :
      OnceClosure(BindOnce(std::forward<Lambda>(lambda))) {}

  void operator()() {
    // TODO(domfarolino): Make this into a CHECK.
    assert(bind_state_);
    bind_state_->Invoke();
    bind_state_.reset();
  }

  operator bool() {
    return !!bind_state_;
  }

 private:
  std::unique_ptr<BindStateBase> bind_state_;
};

template <typename Functor, typename... Args>
OnceClosure BindOnce(Functor&& f, Args&&... args) {
  // Always declare decayed types when passing functors and arguments into
  // `BindState`, so that it retains value semantics. This means that for
  // example, functor arguments passed in as:
  //   - Lvalues --> just get copied normally into `BindState::args_`, as if
  //     there was no type decaying at all.
  //   - Lvalue references --> get demoted to normal values, and copied into
  //     `BindState::args_`
  //   - Rvalue references --> get demoted to normal values, and the
  //     originally-passed-in object is *moved* into the instance of the demoted
  //     type that is stored in, you guessed it, `BindState::args_`. That is if
  //     you pass in std::move(foo) as an argument to `BindOnce()`, the type
  //     will be decayed from `Foo&&` to `Foo&`, and `BindState::args_` will
  //     look like `std::tuple<T1, ..., Foo, ...>`. But we perfectly forward all
  //     of the arguments from this method into `BindState::ctor()`. That way
  //     when we construct the tuple over there, we construct an object of type
  //     `Foo` from an object of type `Foo&&`, and therefore *move* the
  //     originally-passed-in object into an instance stored in the tuple.
  //
  // In order to pass in an lvalue reference and have it be treated as an actual
  // reference at invocation time (thus modifying the value that was originally
  // passed in), you have to declare that you're explicitly passing in an owned
  // reference by using `std::ref()`. This is the same way that `std::bind()`,
  // `std::thread()`, and other APIs work.
  using BindState = BindState<std::decay_t<Functor>, std::decay_t<Args>...>;
  std::unique_ptr<BindStateBase> bind_state =
      std::make_unique<BindState>(
          std::forward<Functor>(f), std::forward<Args>(args)...);
  return OnceClosure(std::move(bind_state));
}

template <typename Lambda>
OnceClosure BindOnce(Lambda&& lambda) {
  return BindOnce<decltype(InvokeLambda)*, Lambda>(InvokeLambda, std::forward<Lambda>(lambda));
}

using Predicate = std::function<bool()>;

} // namespace base

#endif // BASE_CALLBACK_H_
