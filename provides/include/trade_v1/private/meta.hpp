#pragma once

#include "trade_v1/private/access.hpp"

struct trade_v1::Private::meta_t {
  size_t m_access_align_m1;
  size_t m_access_size;
  destroy_t m_destroy;
};

template <class Value> struct trade_v1::Private::meta {
  static const meta_t s_instance;
};

template <class Value>
const trade_v1::Private::meta_t trade_v1::Private::meta<Value>::s_instance = {
    alignof(access_t<Value>) - 1,
    sizeof(access_t<Value>),
    Private::destroy<Value>};
