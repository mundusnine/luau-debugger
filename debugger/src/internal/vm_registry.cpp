#include "vm_registry.h"

namespace luau::debugger {

VMRegistry::~VMRegistry() {
  for (auto L : lua_vms_)
    lua_setthreaddata(L, nullptr);
}

void VMRegistry::registerVM(lua_State* L) {
  lua_vms_.push_back(L);
  markAlive(L, nullptr);
}

bool VMRegistry::isAlive(lua_State* L) const {
  return alive_threads_.find(L) != alive_threads_.end();
}

lua_State* VMRegistry::getParent(lua_State* L) const {
  auto it = alive_threads_.find(L);
  if (it == alive_threads_.end())
    return nullptr;
  return it->second;
}

lua_State* VMRegistry::getRoot(lua_State* L) const {
  lua_State* current = L;
  while (auto* p = getParent(current))
    current = p;
  return current;
}

bool VMRegistry::isChild(lua_State* L, lua_State* parent) const {
  lua_State* current = L;
  while (auto* p = getParent(current)) {
    if (p == parent)
      return true;
    else
      current = p;
  }
  return false;
}

void VMRegistry::markAlive(lua_State* L, lua_State* parent) {
  alive_threads_[L] = parent;
}

void VMRegistry::markDead(lua_State* L) {
  alive_threads_.erase(L);
}
}  // namespace luau::debugger