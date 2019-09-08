#pragma once

#include "trade_v1/private/atom-methods.hpp"
#include "trade_v1/private/non_atomic-methods.hpp"
#include "trade_v1/private/private-methods.hpp"
#include "trade_v1/private/run-methods.hpp"

template <class Value> trade_v1::atom<Value>::atom() {}

template <class Value>
trade_v1::atom<Value>::atom(const Value &value)
    : Private::atom_t<Value>(value) {}

template <class Value> trade_v1::atom<Value>::operator Value() const {
  return Private::load(*this);
}

template <class Value> Value trade_v1::atom<Value>::load() const {
  return Private::load(*this);
}

template <class Value> Value trade_v1::atom<Value>::unsafe_load() const {
  return Private::unsafe_load(*this);
}

template <class Value> Value &trade_v1::atom<Value>::ref() {
  return Private::ref(*this);
}

template <class Value>
template <class Forwardable>
Value &trade_v1::atom<Value>::operator=(Forwardable &&value) {
  return Private::store(*this, std::forward<Forwardable>(value));
}

template <class Value>
template <class Forwardable>
Value &trade_v1::atom<Value>::store(Forwardable &&value) {
  return Private::store(*this, std::forward<Forwardable>(value));
}

template <class Value>
const Value &trade_v1::atom<Value>::unsafe_store(const Value &value) {
  return Private::unsafe_store(*this, value);
}

template <class Config, class Action>
std::invoke_result_t<Action> trade_v1::atomically(Config config,
                                                  Action &&action) {
  return Private::atomically(config, std::forward<Action>(action));
}

template <class Action>
std::invoke_result_t<Action> trade_v1::atomically(Action &&action) {
  return atomically(stack<1024>, std::forward<Action>(action));
}

inline void trade_v1::retry() { Private::retry(Private::s_transaction); }
