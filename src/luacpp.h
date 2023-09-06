#ifndef _LUACPP_H
#define _LUACPP_H

/* This is a C++ API for interacting with Lua objects
 *
 */

#ifndef __cplusplus
#include "lua.h"
#endif

#ifdef __cplusplus
extern "C" {
#include "lua.h"
#include "luajit.h"
}
#include <bit>
#include <stdint.h>
#include <string_view>

namespace lua
{

class value;
class string;
class table;
class mut_table;
class thread;
class scoped_state;
class userdata;
class gcref;

namespace details
{
struct _lj_str;
struct _lj_tab;
struct _lj_udata;
struct _lj_cdata;
template <typename T> struct value_converter;
template <typename T> struct traits;
} // namespace details

class value
{
public:
  value() = default;
  value(const value &o) = default;
  value(lua_State *L, int index);
  value(int32_t v) : value(static_cast<double>(v)) {}
  value(float v) : value(static_cast<double>(v)) {}
  value(double v);

  value(table o);
  value(string o);
  value(userdata o);

  void push(lua_State *L);

  // lua.h LUA_T* constant
  int type() const;

  // Not false or nil, same as lua's if x then ...
  bool istruth() const;
  // Is numeric, does not take into account any conversion tonumber() can apply,
  // does consider CData 64-bit integers as numeric
  bool isnum() const;
  // isnum() and is also integral
  bool isint() const;
  // Is a string, does not take into account any conversion tostring() can apply
  bool isstr() const;
  // Is a table, does not include userdata which may be indexed via metatable
  bool istab() const;

  // Type-safe conversions, use operator bool on return to check for success
  string asstr() const;
  table astab() const;

  // If isnum() is false these return 0
  int64_t asint() const;
  double asnum() const;

  uint32_t hash() const;

  template <typename T> T value_or(T default_value) const
  {
    return details::value_converter<T>::value_or(tv, default_value);
  }

  operator bool() const { return istruth(); }

protected:
  template <unsigned t> void *toobj();

  static value raw(uint64_t tv)
  {
    value v;
    v.tv = tv;
    return v;
  }

  friend string;
  friend table;
  friend userdata;

  uintptr_t tv = ~0ull;
};

class gcref
{
public:
  gcref(value v);
};

class string
{
public:
  string() = default;
  string(const string &o) = default;
  string(value o);
  string(const lua_State *L, std::string_view str);

  operator bool() const { return !!o; }
  operator std::string_view() const { return str(); }
  std::string_view str() const { return std::string_view(c_str(), size()); }

  const char *c_str() const;
  const char *data() const { return c_str(); }
  size_t size() const;

private:
  friend value;
  details::_lj_str *o = nullptr;
};

class table
{
public:
  table() = default;
  table(const table &o) = default;
  table(value o);

  operator bool() const { return !!o; }

  value raw_lookup(value k) const;
  value raw_lookup(unsigned k) const;

  table __index(const lua_State *L) const;
  table getmetatable() const;

  template <typename T> value lookup(const lua_State *L, value k) const
  {
    if (value v = raw_lookup(k))
      return v;
    if (table mt = __index(L))
      return mt.lookup(L, k);
    return value();
  }

  template <typename T> value operator[](T k) const { return raw_lookup(k); }

  struct iterator {
    friend table;

    bool operator==(std::default_sentinel_t) const;

    void operator++()
    {
      idx++;
      find_next();
    }

    std::pair<value, value> operator*() const;

  private:
    iterator(details::_lj_tab *o) : o(o) { find_next(); }
    void find_next();

    details::_lj_tab *o;
    uint32_t idx = 0;
    bool hash = false;
  };
  iterator begin() const { return iterator(o); }
  std::default_sentinel_t end() const { return std::default_sentinel; }

protected:
  table(details::_lj_tab *o) : o(o) {}

  friend value;
  friend userdata;
  details::_lj_tab *o = nullptr;
};

inline string value::asstr() const { return string(*this); }
inline table value::astab() const { return table(*this); }

class mut_table : public table
{
public:
  mut_table() = default;
  mut_table(const mut_table &o) = default;
  explicit mut_table(const lua_State *L);
};

class thread
{
public:
  thread() = default;
  thread(const thread &o) = default;
  thread(lua_State *L) : o(L) {}

  operator lua_State *() const { return o; }
  lua_State *L() { return o; }

protected:
  lua_State *o = nullptr;
};

class userdata
{
public:
  userdata() = default;
  userdata(const userdata &o) = default;
  userdata(value o);

  operator bool() const { return !!o; }
  void *ptr() const;

  void *typed_ptr(uint32_t type) const;

  template<typename T> T *as() const { return (T *)typed_ptr(T::kLuaType); }

  table getmetatable() const;

protected:
  friend value;
  details::_lj_udata *o = nullptr;
};

class scoped_state : public thread
{
public:
  scoped_state() : thread(luaL_newstate()) {}
  scoped_state(lua_Alloc allocf, void *alloc_ud, luaJIT_allocpages allocp,
               luaJIT_freepages freep, luaJIT_reallochuge realloch,
               void *page_ud)
      : thread(
            luaJIT_newstate(allocf, alloc_ud, allocp, freep, realloch, page_ud))
  {
  }
  ~scoped_state() { lua_close(o); }
};

// Everything below this line is implementation details that are specific to the
// exact build associated with this file's revision and not relevant to the
// user. Using any internal details WILL BREAK OR CRASH without warning

} // namespace lua

extern "C" {
#endif
LUA_API uint64_t lj_internal_getstack(lua_State *L, int index);
LUA_API void lj_internal_setstack(lua_State *L, int index, uint64_t tv);
LUA_API void lj_internal_pushraw(lua_State *L, uint64_t tv);
LUA_API void *lj_internal_mt__index(const lua_State *L, void *mt);
LUA_API void *lj_internal_getstr(const lua_State *L, const char *s, size_t n);
LUA_API void *lj_internal_newtab(const lua_State *L);
#ifdef __cplusplus
}

namespace lua
{
namespace details
{
#endif

typedef void *_lj_gcref;

struct _lj_str {
  uint32_t _1;
  _lj_gcref _2;
  uint32_t sid;
  uint32_t _4;
  uint32_t len;
};

struct _lj_node {
  uint64_t v;
  uint64_t k;
  struct _lj_node *next;
};

struct _lj_tab {
  uint32_t _1;
  uint64_t *array;
  struct _lj_node *hash;
  struct _lj_tab *mt;
  _lj_gcref _2;
  uint32_t asize;
  uint32_t hmask;
  _lj_gcref _3;
};

struct _lj_udata {
  uint8_t _1[2];
  uint8_t udtype;
  uint8_t _2;
  uint32_t len;
  void *payload;
  _lj_gcref env;
  struct _lj_tab *mt;
  _lj_gcref _3;
};

struct _lj_cdata {
  uint8_t _1[2];
  uint16_t ctypeid;
  _lj_gcref _2;
  uint16_t offset;
  uint16_t extra;
  uint32_t len;
};

#ifdef __cplusplus
//  ORDER LJ_T
enum {
  Tnil,
  Tfalse,
  Ttrue,
  Tlud,
  Tstr,
  Tuv,
  Tthread,
  Tproto,
  Tfunc,
  Ttrace,
  Tcdata,
  Ttab,
  Tudata,
};

static constexpr uint32_t HASH_BIAS = (-0x04c11db7);
static constexpr uint32_t HASH_ROT1 = 14;
static constexpr uint32_t HASH_ROT2 = 5;
static constexpr uint32_t HASH_ROT3 = 13;

inline uint32_t lj_rol(uint32_t x, uint32_t n)
{
  return (((x) << (n)) | ((x) >> (-(int)(n) & (8 * sizeof(x) - 1))));
}

inline uint32_t hash_alg(uint32_t lo, uint32_t hi)
{
  // see lj_tab.h
#if 1
  /* Prefer variant that compiles well for a 2-operand CPU. */
  lo ^= hi;
  hi = lj_rol(hi, HASH_ROT1);
  lo -= hi;
  hi = lj_rol(hi, HASH_ROT2);
  hi ^= lo;
  hi -= lj_rol(lo, HASH_ROT3);
#else
  lo ^= hi;
  lo = lo - lj_rol(hi, HASH_ROT1);
  hi = lo ^ lj_rol(hi, HASH_ROT1 + HASH_ROT2);
  hi = hi - lj_rol(lo, HASH_ROT3);
#endif
  return hi;
}
} // namespace details

inline value::value(lua_State *L, int index)
    : tv(lj_internal_getstack(L, index))
{
}

// Canonicalize any NaNs
inline value::value(double v)
    : tv((v == v) ? std::bit_cast<uintptr_t>(v) : 0xfff8000000000000ull)
{
}

inline value::value(table o)
    : tv(std::bit_cast<uint64_t>(o.o) | ((uint64_t)~details::Ttab << 47))
{
}
inline value::value(string o)
    : tv(std::bit_cast<uint64_t>(o.o) | ((uint64_t)~details::Tstr << 47))
{
}
inline value::value(userdata o)
    : tv(std::bit_cast<uint64_t>(o.o) | ((uint64_t)~details::Tudata << 47))
{
}

inline void value::push(lua_State *L) { lj_internal_pushraw(L, tv); }

template <unsigned t> inline void *value::toobj()
{
  return ((~tv >> 47) == t) ? (void *)((tv << 17) >> 17) : nullptr;
}

inline int value::type() const
{
  return (0x75a0698042110ull >> (4 * (~tv >> 47))) & 0xF;
}

inline uint32_t value::hash() const
{
  if ((~tv >> 47) == details::Tstr) {
    return ((details::_lj_str *)((tv << 17) >> 17))->sid;
  } else if ((~tv >> 47) >= details::Tudata + 1) {
    // This is a number of some kind
    double v = std::bit_cast<double>(tv);
    long iv = static_cast<long>(v);
    return details::hash_alg((uint32_t)tv, (uint32_t)(tv >> 32) << 1);
  } else {
    return details::hash_alg((uint32_t)tv, (uint32_t)(tv >> 32));
  }
}

inline bool value::istruth() const { return (~tv >> 47) > 1; }
// Is numeric, does not take into account any conversion tonumber() can apply,
// does consider CData 64-bit integers as numeric
// bool value::isnum() const;
// isnum() and is also integral
// bool value::isint() const;
inline bool value::isstr() const { return (~tv >> 47) == details::Tstr; }
inline bool value::istab() const { return (~tv >> 47) == details::Ttab; }

// If isnum() is false these return 0
// int64_t value::asint() const;
// double value::asnum() const;

inline userdata::userdata(value o)
    : o((details::_lj_udata *)o.toobj<details::Tudata>())
{
}

inline void *userdata::ptr() const { return o->payload; }

inline void *userdata::typed_ptr(uint32_t type) const
{
  return (o->udtype == 4 && o->len == type) ? o->payload : nullptr;
}

inline table userdata::getmetatable() const { return table(o->mt); }

inline string::string(value o) : o((details::_lj_str *)o.toobj<details::Tstr>())
{
}

inline string::string(const lua_State *L, std::string_view str)
    : o((details::_lj_str *)lj_internal_getstr(L, str.data(), str.size()))
{
}

inline const char *string::c_str() const { return (const char *)(o + 1); }

inline size_t string::size() const { return o->len; }

inline table::table(value o) : o((details::_lj_tab *)o.toobj<details::Ttab>())
{
}

inline table table::getmetatable() const { return table(o->mt); }

inline value table::raw_lookup(value k) const
{
  uint32_t hash;
  if ((~k.tv >> 47) == details::Tstr) {
    hash = ((details::_lj_str *)((k.tv << 17) >> 17))->sid;
  } else if ((~k.tv >> 47) >= details::Tudata + 1) {
    // This is a number of some kind
    double v = std::bit_cast<double>(k.tv);
    long iv = static_cast<long>(v);
    if (iv >= 0 && iv < (long)o->asize && v == (double)iv) {
      // Integer index
      return value::raw(o->array[iv]);
    }
    hash = details::hash_alg((uint32_t)k.tv, (uint32_t)(k.tv >> 32) << 1);
  } else {
    hash = details::hash_alg((uint32_t)k.tv, (uint32_t)(k.tv >> 32));
  }

  auto n = &o->hash[hash & o->hmask];
  do {
    if (~n->v && n->k == k.tv)
      return value::raw(n->v);
    n = n->next;
  } while (n);
  return value();
}
inline value table::raw_lookup(unsigned k) const
{
  if (k < o->asize)
    return value::raw(o->array[k]);
  return raw_lookup(value((double)k));
}

inline table table::__index(const lua_State *L) const
{
  return table((details::_lj_tab *)lj_internal_mt__index(L, o->mt));
}

inline std::pair<value, value> table::iterator::operator*() const
{
  if (hash)
    return std::make_pair(value::raw(o->hash[idx].k),
                          value::raw(o->hash[idx].v));
  return std::make_pair(value((double)idx), value::raw(o->array[idx]));
}

inline void table::iterator::find_next()
{
  if (!hash) {
    while (idx < o->asize) {
      if (~o->array[idx])
        return;
    }
    hash = true;
    idx = 0;
  }
  while (idx <= o->hmask) {
    if (~o->hash[idx].k && ~o->hash[idx].v)
      return;
  }
}

inline bool table::iterator::operator==(std::default_sentinel_t) const
{
  return hash && idx > o->hmask;
}

inline mut_table::mut_table(const lua_State *L)
    : table((details::_lj_tab *)lj_internal_newtab(L))
{
}

} // namespace lua
#endif

#endif
