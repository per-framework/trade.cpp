#pragma once

#include <atomic>
#include <type_traits>
#include <utility>

namespace trade_core_v2 {

template <class Value> struct atom;

template <class Value> struct atom_traits;

class Private {
  template <class> friend struct ::trade_core_v2::atom;

  template <class> class atom;

  template <template <class> class Derived,
            class Value,
            class Master = Value,
            class Readonly = Value>
  struct basic_traits;

  template <class> struct atomic_traits;
  template <class> struct trivial_traits;
  template <class> struct swappable_traits;

  template <class Value> static constexpr auto trait_of();

public:
  template <class Value> struct atom_traits;
};

} // namespace trade_core_v2

template <class Value> class trade_core_v2::Private::atom {
  template <class> friend struct trade_core_v2::atom;

  using master_type = typename trade_core_v2::atom_traits<Value>::master_type;

  master_type m_master;

  template <class... Forwardable> atom(Forwardable &&... value);
};

//

template <template <class> class Derived,
          class Value,
          class Master,
          class Readonly>
struct trade_core_v2::Private::basic_traits {
  using value_type = Value;
  using readonly_type = Readonly;
  using master_type = Master;

  static constexpr bool is_atomic = false;
  static constexpr bool is_trivial = false;

  static Readonly load(const Master &from);

  static void snap(const Master &from, std::aligned_union_t<1, Value> &to);

  static void commit(Value &from, Master &to) noexcept;
};

template <class Value>
struct trade_core_v2::Private::atomic_traits
    : basic_traits<atomic_traits, Value, std::atomic<Value>> {
  static constexpr bool is_atomic = true;
  static constexpr bool is_trivial = true;

  static Value load(const std::atomic<Value> &from) noexcept;

  static void snap(const std::atomic<Value> &from,
                   std::aligned_union_t<1, Value> &to) noexcept;

  static void commit(Value &from, std::atomic<Value> &to) noexcept;
};

template <class Value>
struct trade_core_v2::Private::trivial_traits
    : basic_traits<trivial_traits, Value> {
  static constexpr bool is_trivial = true;
};

template <class Value>
struct trade_core_v2::Private::swappable_traits
    : basic_traits<swappable_traits, Value> {
  static void commit(Value &from, Value &to) noexcept;
};

//

template <class Value> constexpr auto trade_core_v2::Private::trait_of() {
  static_assert(std::is_copy_constructible_v<Value> &&
                std::is_nothrow_destructible_v<Value> &&
                std::is_nothrow_move_constructible_v<Value>);
  if constexpr (std::is_trivially_copyable_v<Value>) {
    static_assert(std::is_nothrow_copy_assignable_v<Value>);
    if constexpr (std::atomic<Value>::is_always_lock_free) {
      return static_cast<atomic_traits<Value> *>(nullptr);
    } else {
      return static_cast<trivial_traits<Value> *>(nullptr);
    }
  } else {
    static_assert(std::is_nothrow_swappable_v<Value>);
    return static_cast<swappable_traits<Value> *>(nullptr);
  }
}

template <class Value>
struct trade_core_v2::Private::atom_traits
    : std::remove_pointer_t<decltype(trait_of<Value>())> {};
