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

template <class Config, class Action>
std::invoke_result_t<Action> trade_v1::atomically(Config config,
                                                  Action &&action) {
  return Private::s_transaction
             ? action()
             : Private::run_t<
                   std::conditional_t<std::is_same_v<Config, heap>,
                                      Private::transaction_heap_t,
                                      Private::transaction_stack_t<Config>>,
                   std::invoke_result_t<Action>>::run(config, action);
}

template <class Action>
std::invoke_result_t<Action> trade_v1::atomically(Action &&action) {
  return atomically(stack<1024>, action);
}

inline void trade_v1::retry() { Private::retry(Private::s_transaction); }
