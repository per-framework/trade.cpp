#pragma once

#include "trade_v1/private/private.hpp"

struct trade_v1::Private::access_base_t {
  access_base_t *m_children[2];
  atom_mono_t *m_atom;
  state_t m_state;
  lock_ix_t m_lock_ix;
  destroy_t m_destroy;
};

template <class Value>
struct trade_v1::Private::access_t<Value, false> : access_base_t {
  ~access_t() = delete;

  Value m_current;
  Value m_original;

  void retain_copy();
  void retain_move();
  void destroy();
};

template <class Value>
struct trade_v1::Private::access_t<Value, true> : access_base_t {
  ~access_t() = delete;

  Value m_current;

  void retain_copy();
  void retain_move();
  void destroy();
};
