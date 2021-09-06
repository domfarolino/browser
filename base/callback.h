#ifndef BASE_CALLBACK_H_
#define BASE_CALLBACK_H_

#include <functional>
#include <memory>
#include <tuple>

// Sources:
//   - https://stackoverflow.com/questions/16868129
//   - https://stackoverflow.com/questions/66501654

namespace base {

// `BindStateBase` only exists for type erasure -- that is, to allow `Closure`
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
 public:
  BindState() = delete;
  BindState(Functor&& f, Args&&... args) :
    f_(std::forward<Functor>(f)), args_(std::forward<Args>(args)...) {}
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
  void InvokeImpl(BoundFunctor&& f, BoundArgs&& args, std::index_sequence<Is...>) {
    // TODO(domfarolino): Figure out why this doesn't actually *move* from `args_`.
    std::forward<BoundFunctor>(f)(std::get<Is>(std::forward<BoundArgs>(args)) ...);
  }

 private:
  Functor f_;
  std::tuple<Args...> args_;
};

// The `Closure` class is just a light wrapper around `BindStateBase`. Once we
// introduce `Callback`, `Closure` will be a specialization of `Callback`, and
// `Callback` will support partially-bound functors.
class Closure {
 public:
  explicit Closure(std::unique_ptr<BindStateBase> bind_state) :
      bind_state_(std::move(bind_state)) {}
  Closure(const Closure& other) = delete;
  Closure& operator=(const Closure& other) = delete;
  Closure(Closure&& other) = default;
  Closure& operator=(Closure&& other) = default;

  // TODO(domfarolino): This is kind of weird, it basically eliminates the need
  // for explicit BindOnce().
  template <typename Functor, typename... Args>
  Closure(Functor&& f, Args&&... args) :
      bind_state_(std::make_unique<BindState<Functor, Args...>>(
                      std::forward<Functor>(f), std::forward<Args>(args)...)) {}

  void operator()() {
    // TODO(domfarolino): Make this into a CHECK.
    assert(bind_state_);
    bind_state_->Invoke();
  }

 private:
  std::unique_ptr<BindStateBase> bind_state_;
};

template <typename Functor, typename... Args>
Closure BindOnce(Functor&& f, Args&&... args) {
  std::unique_ptr<BindStateBase> bind_state =
      std::make_unique<BindState<Functor, Args...>>(
          std::forward<Functor>(f), std::forward<Args>(args)...);
  return Closure(std::move(bind_state));
}

using Callback = std::function<void()>;
using Predicate = std::function<bool()>;

} // namespace base

#endif // BASE_CALLBACK_H_
