#pragma once

#include "testing/config.hpp"

#include "trade_v1/trade.hpp"

#include "dumpster_v1/primes.hpp"

#include "polyfill_v1/memory.hpp"
#include <functional>
#include <optional>
#include <utility>

namespace testing {

/// A transactional hash map for testing purposes.
template <class Key,
          class Mapped,
          class Hash = std::hash<Key>,
          class Equal = std::equal_to<Key>>
class hash_map_tm;

class hash_map_tm_private {
  template <class, class, class, class> friend class hash_map_tm;

  // This hack is a workaround for not having std::shared_ptr<T[]> support
  // in AppleClang.
  template <class T> struct array_hack {
    void operator delete(void *self) { delete[] reinterpret_cast<T *>(self); }
    T &at(size_t i) { return reinterpret_cast<T *>(this)[i]; }
  };
};

template <class Key, class Mapped, class Hash, class Equal>
class hash_map_tm : hash_map_tm_private {
  template <class T> using ptr_t = std::shared_ptr<T>;

  struct node_t;

  using link = trade::atom<std::shared_ptr<node_t>>;

  trade::atom<size_t> m_item_count;
  trade::atom<size_t> m_buckets_count;
  trade::atom<ptr_t<array_hack<link>>> m_buckets;

public:
  using size_type = size_t;

  using key_type = Key;
  using mapped_type = Mapped;

  hash_map_tm();

  hash_map_tm(const hash_map_tm &) = delete;
  hash_map_tm &operator=(const hash_map_tm &) = delete;

  size_t size() const;

  bool empty() const;

  void clear();

  void swap(hash_map_tm &that);

  template <class ForwardableMapped, class Config = trade::stack_t<1024>>
  bool add_or_set(const Key &key,
                  ForwardableMapped &&mapped,
                  Config config = trade::stack<1024>);

  std::optional<Mapped> try_get(const Key &key) const;

  bool remove(const Key &key);

#ifndef NDEBUG
  static std::atomic<size_t> s_live_nodes; // Only for testing purposes
#endif
};

// -----------------------------------------------------------------------------

template <class Key, class Mapped, class Hash, class Equal>
struct hash_map_tm<Key, Mapped, Hash, Equal>::node_t {
#ifndef NDEBUG
  ~node_t() { --s_live_nodes; }
#endif
  template <class ForwardableKey, class ForwardableMapped>
  node_t(ForwardableKey &&key, ForwardableMapped &&value)
      : m_next(nullptr), m_key(std::forward<ForwardableKey>(key)),
        m_mapped(std::forward<ForwardableMapped>(value)) {
#ifndef NDEBUG
    ++s_live_nodes;
#endif
  }
  link m_next;
  const Key m_key;
  trade::atom<Mapped> m_mapped;
};

//

template <class Key, class Mapped, class Hash, class Equal>
hash_map_tm<Key, Mapped, Hash, Equal>::hash_map_tm()
    : m_item_count(0), m_buckets_count(0), m_buckets(nullptr) {}

template <class Key, class Mapped, class Hash, class Equal>
size_t hash_map_tm<Key, Mapped, Hash, Equal>::size() const {
  return trade::atomically(trade::assume_readonly,
                           [&]() { return m_item_count.load(); });
}

template <class Key, class Mapped, class Hash, class Equal>
bool hash_map_tm<Key, Mapped, Hash, Equal>::empty() const {
  return trade::atomically(trade::assume_readonly,
                           [&]() { return m_item_count == 0; });
}

template <class Key, class Mapped, class Hash, class Equal>
void hash_map_tm<Key, Mapped, Hash, Equal>::clear() {
  trade::atomically([&]() {
    m_item_count = 0;
    m_buckets_count = 0;
    m_buckets = nullptr;
  });
}

template <class Key, class Mapped, class Hash, class Equal>
void hash_map_tm<Key, Mapped, Hash, Equal>::swap(hash_map_tm &that) {
  trade::atomically([&]() {
    std::swap(m_item_count.ref(), that.m_item_count.ref());
    std::swap(m_buckets_count.ref(), that.m_buckets_count.ref());
    std::swap(m_buckets.ref(), that.m_buckets.ref());
  });
}

template <class Key, class Mapped, class Hash, class Equal>
template <class ForwardableMapped, class Config>
bool hash_map_tm<Key, Mapped, Hash, Equal>::add_or_set(
    const Key &key, ForwardableMapped &&mapped, Config config) {
  auto key_hash = Hash()(key);

  return trade::atomically(config, [&]() {
    auto item_count = m_item_count.load();
    auto buckets_count = m_buckets_count.load();
    auto buckets = m_buckets.load();

    if (buckets_count <= item_count) {
      auto old_buckets = std::move(buckets);
      auto old_buckets_count = buckets_count;

      m_buckets_count = buckets_count =
          dumpster::prime_less_than_next_pow_2_or_1(old_buckets_count * 2 + 1);
      m_buckets = buckets = ptr_t<array_hack<link>>(
          reinterpret_cast<array_hack<link> *>(new link[buckets_count]));

      for (size_t i = 0; i < old_buckets_count; ++i) {
        auto work = old_buckets->at(i).load();
        while (work) {
          auto &ref_next = work->m_next.ref();
          auto &ref_bucket =
              buckets->at(Hash()(work->m_key) % buckets_count).ref();
          auto next = std::move(ref_next);
          ref_next = std::move(ref_bucket);
          ref_bucket = std::move(work);
          work = std::move(next);
        }
      }
    }

    auto prev = &buckets->at(key_hash % buckets_count);
    while (true) {
      if (auto node = prev->load()) {
        if (Equal()(node->m_key, key)) {
          node->m_mapped = std::forward<ForwardableMapped>(mapped);
          return false;
        } else {
          prev = &node->m_next;
        }
      } else {
        prev->ref().reset(
            new node_t(key, std::forward<ForwardableMapped>(mapped)));
        m_item_count = item_count + 1;
        return true;
      }
    }
  });
}

template <class Key, class Mapped, class Hash, class Equal>
std::optional<Mapped>
hash_map_tm<Key, Mapped, Hash, Equal>::try_get(const Key &key) const {
  auto key_hash = Hash()(key);
  return trade::atomically(
      trade::assume_readonly, [&]() -> std::optional<Mapped> {
        if (auto buckets_count = m_buckets_count.load())
          for (auto node =
                   m_buckets.load()->at(key_hash % buckets_count).load();
               node;
               node = node->m_next)
            if (Equal()(node->m_key, key))
              return node->m_mapped.load();
        return std::nullopt;
      });
}

template <class Key, class Mapped, class Hash, class Equal>
bool hash_map_tm<Key, Mapped, Hash, Equal>::remove(const Key &key) {
  auto key_hash = Hash()(key);
  return trade::atomically([&]() {
    if (auto buckets_count = m_buckets_count.load()) {
      auto prev = &m_buckets.load()->at(key_hash % buckets_count);
      while (true) {
        auto node = prev->load();
        if (!node)
          break;
        if (Equal()(node->m_key, key)) {
          *prev = node->m_next;
          return true;
        }
        prev = &node->m_next;
      }
    }
    return false;
  });
}

#ifndef NDEBUG
template <class Key, class Mapped, class Hash, class Equal>
std::atomic<size_t> hash_map_tm<Key, Mapped, Hash, Equal>::s_live_nodes = 0;
#endif

} // namespace testing
