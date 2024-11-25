#include <internal/utils.h>
#include <internal/utils/lua_utils.h>
#include <lua.h>
#include <optional>

namespace luau::debugger::lua_utils {

std::optional<int> eval(lua_State* L, const std::string& code, int env) {
  auto main_vm = lua_mainthread(L);
  auto callbacks = lua_callbacks(main_vm);
  auto old_step = callbacks->debugstep;

  auto _ = utils::makeRAII(
      [callbacks]() { callbacks->debugstep = nullptr; },
      [callbacks, old_step]() { callbacks->debugstep = old_step; });

  lua_setsafeenv(L, LUA_ENVIRONINDEX, false);
  auto env_idx = lua_absindex(L, env);
  int top = lua_gettop(L);

  Luau::BytecodeBuilder bcb;
  try {
    Luau::compileOrThrow(bcb, std::string("return ") + code);
  } catch (const std::exception&) {
    try {
      Luau::compileOrThrow(bcb, code);
    } catch (const std::exception& e) {
      DEBUGGER_LOG_ERROR("Error compiling code: {}", e.what());
      lua_pushstring(L, e.what());
      return std::nullopt;
    }
  }

  auto bytecode = bcb.getBytecode();
  int result = luau_load(L, code.c_str(), bytecode.data(), bytecode.size(), 0);
  if (result != 0)
    return std::nullopt;

  lua_pushvalue(L, env_idx);
  lua_setfenv(L, -2);

  int call_result = lua_pcall(L, 0, LUA_MULTRET, 0);
  if (call_result == LUA_OK)
    return lua_gettop(L) - top;
  else {
    DEBUGGER_LOG_ERROR("Error running code: {}", lua_tostring(L, -1));
    return std::nullopt;
  }
}

std::string toString(lua_State* L, int index) {
  auto type = lua_type(L, index);
  std::string value;
  switch (type) {
    case LUA_TNIL:
      value = "nil";
      break;
    case LUA_TBOOLEAN:
      value = lua_toboolean(L, index) ? "true" : "false";
      break;
    case LUA_TVECTOR: {
      const float* v = lua_tovector(L, index);
      value = std::format("({}, {}, {})", v[0], v[1], v[2]);
      break;
    }
    case LUA_TNUMBER:
      // NOTICE: make lua_tostring call to a copy of the value to avoid modify
      // the original value
      lua_checkstack(L, 1);
      lua_pushvalue(L, index);
      value = lua_tostring(L, -1);
      lua_pop(L, 1);
      break;
    case LUA_TSTRING:
      value = lua_tostring(L, index);
      break;
    case LUA_TLIGHTUSERDATA:
      value = "lightuserdata";
      break;
    case LUA_TTABLE:
      value = "table";
      break;
    case LUA_TFUNCTION:
      value = "function";
      break;
    case LUA_TUSERDATA:
      value = "userdata";
      break;
    case LUA_TTHREAD:
      value = "thread";
      break;
    case LUA_TBUFFER:
      value = "buffer";
      break;
    default:
      value = "unknown";
      break;
  };
  return value;
}

bool pushBreakEnv(lua_State* L, int level) {
  lua_Debug ar;
  lua_checkstack(L, 5);

  // Create new table for break environment
  lua_newtable(L);

  // Push function at level
  if (!lua_getinfo(L, level, "f", &ar)) {
    DEBUGGER_LOG_ERROR("[pushBreakEnv] Failed to get function info at level {}",
                       level);
    lua_pop(L, 1);
    return false;
  }

  // Get function env
  lua_getfenv(L, -1);

  // Copy function env to break env
  // -1: function env table
  // -2: function
  // -3: break env table
  int break_env = lua_absindex(L, -3);
  lua_pushnil(L);
  while (lua_next(L, -2)) {
    lua_pushvalue(L, -2);
    lua_pushvalue(L, -2);
    lua_settable(L, break_env);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  // -1: function
  // -2: table

  int index = 1;
  while (auto* name = lua_getlocal(L, level, index++)) {
    lua_pushstring(L, name);
    lua_insert(L, -2);

    // -1: value
    // -2: key
    // -3: function
    // -4: table
    lua_rawset(L, -4);
  }

  index = 1;
  while (auto* name = lua_getupvalue(L, -1, index++)) {
    lua_pushstring(L, name);
    lua_insert(L, -2);
    lua_rawset(L, -4);
  }

  // Pop function
  lua_pop(L, 1);

  return true;
}

bool setLocal(lua_State* L, int level, const std::string& name, int index) {
  auto value_idx = lua_absindex(L, index);
  int n = 1;
  while (const char* local_name = lua_getlocal(L, level, n)) {
    if (name == local_name) {
      lua_pushvalue(L, value_idx);
      auto* result = lua_setlocal(L, level, n);
      lua_pop(L, 1);
      return result != nullptr;
    }
    lua_pop(L, 1);
    ++n;
  }
  return false;
}

bool setUpvalue(lua_State* L, int level, const std::string& name, int index) {
  auto value_idx = lua_absindex(L, index);
  lua_Debug ar;
  lua_checkstack(L, 1);
  if (!lua_getinfo(L, level, "f", &ar)) {
    DEBUGGER_LOG_ERROR("[setUpvalue] Failed to get function info at level {}",
                       level);
    return false;
  }
  auto function_index = lua_absindex(L, -1);

  int n = 1;
  while (const char* local_name = lua_getupvalue(L, function_index, n)) {
    if (name == local_name) {
      lua_pushvalue(L, value_idx);
      auto* result = lua_setupvalue(L, function_index, n);
      DEBUGGER_ASSERT(result != nullptr);
      lua_pop(L, 2);
      return result != nullptr;
    }
    lua_pop(L, 1);
    ++n;
  }
  lua_pop(L, 1);
  return false;
}

}  // namespace luau::debugger::lua_utils