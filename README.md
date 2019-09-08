# [≡](#contents) [Trade.C++](#) [![Gitter](https://badges.gitter.im/per-framework/community.svg)](https://gitter.im/per-framework/community) [![Build Status](https://travis-ci.org/per-framework/trade.cpp.svg?branch=v1)](https://travis-ci.org/per-framework/trade.cpp) [![Code Coverage](https://img.shields.io/codecov/c/github/per-framework/trade.cpp/v1.svg)](https://codecov.io/gh/per-framework/trade.cpp/branch/v1)

A transactional locking implementation for C++. Based on basic ideas from

<dl>
<dt><a href="https://perso.telecom-paristech.fr/kuznetso/INF346/papers/tl2.pdf">Transactional Locking II</a></dt>
<dd>Dave Dice, Ori Shalev, and Nir Shavit</dd>
</dl>

this simple variation uses a hash table of locks, a
[splay tree](https://en.wikipedia.org/wiki/Splay_tree) to maintain an access
set, and attempts locks in address order.

In a nutshell one could describe transactional locking as allowing you to write
concurrent and parallel programs roughly as easily as if you only had a single
global [mutex](https://en.cppreference.com/w/cpp/thread/mutex) and a single
global
[condition variable](https://en.cppreference.com/w/cpp/thread/condition_variable)
while providing a performance profile more like that of highly fine-grained
locking.

The goal of this library is to provide an API that makes transactional locking
work like a natural part of C++ and a portable implementation that can be used
_today_ for prototyping parallel algorithms using
[transactional memory](https://en.wikipedia.org/wiki/Software_transactional_memory).

See [`synopsis.hpp`](provides/include/trade_v1/synopsis.hpp) for the API and
[`queue_tm.hpp`](internals/include/testing/queue_tm.hpp) for a transactional
queue implementation written for testing purposes.

## <a id="contents"></a> [≡](#contents) [Contents](#contents)

- [Overview](#overview)
  - [Building](#building)
  - [Basics](#basics)
  - [Ref](#ref)
  - [Side-effects](#side-effects)
  - [Nesting](#nesting)
  - [Blocking](#blocking)
  - [Memory management](#memory-management)
  - [Readonly transactions](#readonly-transactions)
  - [Stack or heap allocation](#stack-or-heap-allocation)
  - [Atomic types only](#atomic-types-only)
  - [Exceptions](#exceptions)
- [Trade-offs](#trade-offs)

## <a id="overview"></a> [≡](#contents) [Overview](#overview)

### <a id="building"></a> [≡](#contents) [Building](#building)

This project uses the [C++ submodule manager](https://cppsm.github.io/). If you
have the `cppsm` command in path, you can just clone and test this project:

```bash
cppsm clone "git@github.com:per-framework/trade.cpp.git" v1
cd trade.cpp
cppsm test
```

Without the [C++ submodule manager](https://cppsm.github.io/) you need to
**non-recursively** update the submodules after cloning and use
[CMake](https://cmake.org/) to e.g. generate makefiles and use
[make](https://www.gnu.org/software/make/) to build and test:

```bash
git clone git@github.com:per-framework/trade.cpp.git
cd trade.cpp
git submodule update --init
mkdir .build
cd .build
cmake ..
make all test
```

### <a id="basics"></a> [≡](#contents) [Basics](#basics)

Using this library one stores values in `atom`s that can then be accessed from
any number of threads transactionally within `atomically` blocks.

For example, given

```c++
atom<int> xA = 1;
atom<int> yA = 2;
```

we could write

```c++
int sum = atomically([&]() {
  int x = xA;
  int y = yA;
  yA = x;
  xA = y;
  return x + y;
});
```

to atomically, with respect to other transactions, swap the values of `xA` and
`yA` and compute their sum. Loads of and stores to atoms can only be made inside
`atomically` blocks.

### <a id="ref"></a> [≡](#contents) [Ref](#ref)

As a convenience and optimization a mutable reference to the current value of an
atom within a transaction can be obtained with `ref()`.

For example, one could write

```c++
atomically([&]() { counter.ref() += 1; });
```

to increment the value of a `counter` atom.

The returned reference is only valid within the transaction and can be used
multiple times.

### <a id="side-effects"></a> [≡](#contents) [Side-effects](#side-effects)

The action given to `atomically` may be invoked many times. Therefore it is
important that the action does not perform any
[side-effects](<https://en.wikipedia.org/wiki/Side_effect_(computer_science)>)
that must not be repeated.

On the other hand, the action will only be invoked from the thread that started
the transaction. So, it is entirely possible to use side-effects safely within
the action.

For example, given a
[queue](<https://en.wikipedia.org/wiki/Queue_(abstract_data_type)>) with
transactional `empty` predicate and `pop_front` operation, one could write

```c++
std::vector<Value> values;

atomically([&]() {
  values.clear();
  while (!queue.empty())
    values.push_back(queue.pop_front());
});
```

to transactionally pop the values off of the queue and push them into a
[vector](https://en.cppreference.com/w/cpp/container/vector). It is important
that the `values` vector is cleared at the beginning of the action.

### <a id="nesting"></a> [≡](#contents) [Nesting](#nesting)

`atomically` blocks can be nested and thereby transactions composed.

For example, given a queue with transactional `try_pop_front` and `push_back`
operations, code such as

```c++
bool moved = atomically([&]() {
  if (auto opt_v = q1.try_pop_front()) {
    q2.push_back(opt_v.value());
    return true;
  }
  return false;
});
```

could transactionally move an element from one queue to another such that no
other transaction may observe a state where the element is not in either queue.

### <a id="blocking"></a> [≡](#contents) [Blocking](#blocking)

An atomically block can call `retry` at any point to stop running the
transaction and [block](<https://en.wikipedia.org/wiki/Blocking_(computing)>)
waiting for other threads to make changes to atoms read during the transaction.
Once such changes have been made, the transaction will be restarted.

For example, given a queue with a transactional `try_pop_front` operation
returning an optional value, one could write

```c++
auto value = atomically([&]() {
  if (auto opt_value = queue.try_pop_front())
    return opt_value.value();
  retry();
});
```

to wait until a value can be obtained from the queue.

### <a id="memory-management"></a> [≡](#contents) [Memory management](#memory-management)

Care must be taken when dynamically allocated memory is accessed within
transactions &mdash; objects that contain atoms must not be deallocated
prematurely.

To support dynamic memory management, loads within transactions take and keep a
copy of the original value in the atom. This allows smart pointers such as
`std::shared_ptr` to be used for memory management as long as there is a
suitable
[partial specialization of `std::atomic`](https://en.cppreference.com/w/cpp/memory/shared_ptr/atomic2).

For example, a transactional dynamic queue data structure could be defined with
the help of `std::shared_ptr` as

```c++
template <class Value> class queue_tm {
  struct node_t {
    atom<std::shared_ptr<node_t>> m_next;
    Value m_value;
    node_t(const Value &value) : m_value(value) {}
  };

  atom<std::shared_ptr<node_t>> m_first;
  atom<std::shared_ptr<node_t>> m_last;

  // ...
};
```

A `push_back` operation for queues could then be implemented as

```c++
void push_back(const value_t &value) {
  std::shared_ptr<node_t> node(new node_t(value));
  atomically([&]() {
    if (auto prev = m_last.load())
      prev->m_next = m_last = node;
    else
      m_first = m_last = node;
  });
}
```

and a `pop_front` operation as

```c++
value_t pop_front() {
  return atomically([&]() {
    if (auto first = m_first.load()) {
      if (auto next = first->m_next.load())
        m_first = next;
      else
        m_last = m_first = nullptr;
      return node->m_value;
    }
    retry();
  });
}
```

### <a id="readonly-transactions"></a> [≡](#contents) [Readonly transactions](#readonly-transactions)

Transactions that are readonly and do not write into atoms can be executed more
efficiently without creating a transaction log. To run a readonly transaction,
simply pass `assume_readonly` to `atomically`.

For example, to compute the size of a transactional queue, one could write

```c++
size_t size() {
  return atomically(assume_readonly, [&]() {
    size_t n = 0;
    for (auto node = m_first.load(); node; node = node->m_next)
      n += 1;
    return n;
  });
}
```

Note that algorithmically the above may not be a good idea as it goes through
the entire queue.

### <a id="stack-or-heap-allocation"></a> [≡](#contents) [Stack or heap allocation](#stack-or-heap-allocation)

Loads and stores of atoms inside transactions create a transaction log. The
memory for the log can be allocated either from a fixed size buffer on the stack
or dynamically from the heap.

By default the log is allocated from the stack with a capacity that should be
sufficient for most common use cases where a small number of atoms are accessed.
When necessary, pass to `atomically` either `heap(initial_size)` to specify heap
allocation or `stack<fixed_size>` to specify stack allocation.

For example, one could specify `stack<64>` to execute a tiny transaction as

```c++
atomically(stack<64>, [&]() { atom = 101; });
```

and avoid wasting stack.

In case a transaction runs out of log space, the transaction will be aborted by
raising the
[`std::bad_alloc`](https://en.cppreference.com/w/cpp/memory/new/bad_alloc)
exception.

Note that only the allocation configuration of the outermost `atomically` call
is considered in [nested transactions](#nesting).

### <a id="atomic-types-only"></a> [≡](#contents) [Atomic types only](#atomic-types-only)

The type argument `T` of `atom<T>` must also be allowed as an argument to
[`std::atomic`](https://en.cppreference.com/w/cpp/atomic/atomic). Unless the
type `T` is
[TriviallyCopyable](https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable)
and `std::atomic<T>` is not always lock-free, an atom stores the value in a
`std::atomic<T>`. This allows efficient implementation of the necessary
atomicity guarantees when multiple threads may simultaneously access the value
stored in an atom.

### <a id="exceptions"></a> [≡](#contents) [Exceptions](#exceptions)

Invalid accesses and [`retry`](#blocking) raise exceptions. User code inside
transactions should let such exceptions fall through to the handler in the
outermost `atomically` block. The action given to `atomically` may throw other
exceptions &mdash; such exceptions will be allowed to fall through and abort the
transaction.

## <a id="trade-offs"></a> [≡](#contents) [Trade-offs](#trade-offs)

- A portable implementation _usable today_ with any C++17 compiler. If
  necessary, it should be fairly easy to support plain C++11.

- As in the
  [TL2](https://perso.telecom-paristech.fr/kuznetso/INF346/papers/tl2.pdf)
  algorithm,

  - a shared global clock is used, which may cause significant contention for
    tiny transactions,

  - does not require use of special heap for transactional memory, although user
    code is required to cooperate to avoid premature deallocations, and

  - user code cannot observe inconsistent memory states.

- The use of hashed locks may cause
  [false sharing](https://en.wikipedia.org/wiki/False_sharing) (as multiple
  atoms may hash to a single lock) and in the unlikely worst case performance
  may degrade to the point where it is equivalent to having a single global
  lock.

- The hash computation adds some overhead to every access.

- On the other hand, the size of an `atom<T>` is not larger than the size of
  `std::atomic<T>`, which is practically optimal.

- Adds a [`retry`](#blocking) capability for blocking. This also takes minor
  advantage of the use of hashed locks.

- Every access of an atom potentially involves loading a
  [thread-local variable](https://en.wikipedia.org/wiki/Thread-local_storage).
  Ideally a good compiler would be able to eliminate many thread-local accesses,
  but that does not seem to happen. This adds a certain amount of overhead to
  accesses.

- The use of exceptions for aborting transactions may be expensive and may also
  prohibit or complicate use in cases like embedded systems where exception
  handling might be turned off to reduce code size.
