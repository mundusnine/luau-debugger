#pragma once

#include <dap/protocol.h>
#include <dap/typeof.h>
#include <nlohmann_json_serializer.h>
#include <concepts>

#include "internal/log.h"

namespace luau::debugger::utils {

template <class T>
concept Serializable =
    std::derived_from<T, dap::Request> || std::derived_from<T, dap::Event> ||
    std::derived_from<T, dap::Response>;
template <Serializable T>
inline std::string toString(const T& t) {
  dap::json::NlohmannSerializer s;
  dap::TypeOf<T>::type()->serialize(&s, reinterpret_cast<const void*>(&t));
  return s.dump();
}
}  // namespace luau::debugger::utils