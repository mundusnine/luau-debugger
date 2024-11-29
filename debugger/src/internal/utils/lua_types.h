#pragma once
#include <format>
#include <string>

#include <lua.h>

namespace luau::debugger::lua_utils::type {

template <class T>
concept Type = requires(T, lua_State* L, int index) {
  { T::type } -> std::convertible_to<lua_Type>;
  { T::toString(L, index) } -> std::same_as<std::string>;
};

template <Type... T>
struct TypeList;

class Nil {
 public:
  static constexpr lua_Type type = LUA_TNIL;
  static std::string toString(lua_State* L, int index) { return "nil"; }
};

class Boolean {
 public:
  static constexpr lua_Type type = LUA_TBOOLEAN;
  static std::string toString(lua_State* L, int index) {
    return lua_toboolean(L, index) ? "true" : "false";
  }
};

class Vector {
 public:
  static constexpr lua_Type type = LUA_TVECTOR;
  static std::string toString(lua_State* L, int index) {
    const float* v = lua_tovector(L, index);
    return std::format("({}, {}, {})", v[0], v[1], v[2]);
  }
};

class Number {
 public:
  static constexpr lua_Type type = LUA_TNUMBER;
  static std::string toString(lua_State* L, int index) {
    // NOTICE: make lua_tostring call to a copy of the value to avoid modify
    // the original value
    lua_checkstack(L, 1);
    lua_pushvalue(L, index);
    std::string value = lua_tostring(L, -1);
    lua_pop(L, 1);
    return value;
  }
};

class String {
 public:
  static constexpr lua_Type type = LUA_TSTRING;
  static std::string toString(lua_State* L, int index) {
    return lua_tostring(L, index);
  }
};

class LightUserData {
 public:
  static constexpr lua_Type type = LUA_TLIGHTUSERDATA;
  static std::string toString(lua_State* L, int index) {
    return "lightuserdata";
  }
};

class Table {
 public:
  static constexpr lua_Type type = LUA_TTABLE;
  static std::string toString(lua_State* L, int index) { return "table"; }
};

class Function {
 public:
  static constexpr lua_Type type = LUA_TFUNCTION;
  static std::string toString(lua_State* L, int index) { return "function"; }
};

class UserData {
 public:
  static constexpr lua_Type type = LUA_TUSERDATA;
  static std::string toString(lua_State* L, int index) { return "userdata"; }
};

class Thread {
 public:
  static constexpr lua_Type type = LUA_TTHREAD;
  static std::string toString(lua_State* L, int index) { return "thread"; }
};

class Buffer {
 public:
  static constexpr lua_Type type = LUA_TBUFFER;
  static std::string toString(lua_State* L, int index) { return "buffer"; }
};

using RegisteredTypes = TypeList<Nil,
                                 Boolean,
                                 Vector,
                                 Number,
                                 String,
                                 LightUserData,
                                 Table,
                                 Function,
                                 UserData,
                                 Thread,
                                 Buffer>;

std::string toString(lua_State* L, int index);

}  // namespace luau::debugger::lua_utils::type