#pragma once

#include "trade_v1/private/atom.hpp"

template <class Value> trade_v1::Private::atom_t<Value>::atom_t() {}

template <class Value>
trade_v1::Private::atom_t<Value>::atom_t(const Value &value) : m_value(value) {}
