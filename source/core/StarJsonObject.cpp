#include "StarJsonObject.hpp"
#include "StarJson.hpp"
#include "StarDataStream.hpp"

namespace Star {

pair<JsonObjectConstIterator, bool> JsonObject::insert(pair<String, Json> const& p) {
  auto result = m_map.insert(make_pair(StringInterner::instance().intern(p.first), p.second));
  return make_pair(JsonObjectConstIterator(static_cast<void*>(&result.first)), result.second);
}

pair<JsonObjectConstIterator, bool> JsonObject::insert(String const& k, Json const& v) {
  auto result = m_map.insert(make_pair(StringInterner::instance().intern(k), v));
  return make_pair(JsonObjectConstIterator(static_cast<void*>(&result.first)), result.second);
}

JsonObject::JsonObject(std::initializer_list<std::pair<String, Json>> init) {
  for (auto const& p : init) {
    insert(p.first, p.second);
  }
}

JsonObject::JsonObject(StringMap<Json> const& map) {
  for (auto const& pair : map) {
    insert(pair.first, pair.second);
  }
}

StringMap<Json> JsonObject::toStringMap() {
  StringMap<Json> map;
  for (auto const& p : m_map) {
    map.add(p.first.toString(), p.second);
  }
  return map;
}

size_t JsonObject::size() const { 
  return m_map.size(); 
}

void JsonObject::reserve(size_t capacity) { 
  return m_map.reserve(capacity); 
}

void JsonObject::clear() { 
  return m_map.clear(); 
}

bool JsonObject::empty() const { 
  return m_map.empty(); 
}

bool JsonObject::remove(String const& key) { 
  return m_map.remove(StringInterner::instance().intern(key)); 
}

pair<JsonObjectConstIterator, bool> JsonObject::add(String const& key, Json const& value) { 
  pair<JsonObjectConstIterator, bool> pair = insert(std::move(key), std::move(value));
  if (!pair.second)
    throw MapException(strf("Entry with key '{}' already present.", outputAny(key)));
  else
    return pair;
}

List<pair<String, Json>> JsonObject::pairs() {
  List<pair<String, mapped_type>> plist;
  for (auto const& kv : m_map)
    plist.push_back(make_pair(kv.first.toString(), kv.second));
  return plist;
}

bool JsonObject::erase(String const& key) {
  auto handle = StringInterner::instance().intern(key);
  // returns number of erased items (0 or 1 for a typical HashMap)
  size_t n = m_map.erase(handle);
  return n > 0;
}

pair<String, Json> JsonObject::first() const {
  auto it = m_map.begin();
  if (it == m_map.end())
    throw StarException("JsonObject::first() called on empty object");

  String realKey = StringInterner::instance().lookup(it->first.id);
  return make_pair(std::move(realKey), it->second);
}

List<String> JsonObject::keys() const {
  List<String> result;
  result.reserve(m_map.size());
  for (auto const& kv : m_map) {
    result.append(StringInterner::instance().lookup(kv.first.id));
  }
  return result;
}

List<Json> JsonObject::values() const {
  List<Json> result;
  result.reserve(m_map.size());
  for (auto const& kv : m_map) {
    result.append(kv.second);
  }
  return result;
}

bool JsonObject::contains(String const& key) const {
  auto handle = StringInterner::instance().intern(key);
  return m_map.find(handle) != m_map.end();
}


bool JsonObject::merge(StringMap<Json> const& sourceMap, bool overwrite) {
  bool noCommonKeys = true;

  for (auto const& kv : sourceMap) {
    auto res = insert(kv);

    if (!res.second) {
      noCommonKeys = false;
      if (overwrite) {
        res.first->second = kv.second;
      }
    }
  }
  return noCommonKeys;
}

bool JsonObject::merge(JsonObject const& sourceMap, bool overwrite) {
  bool noCommonKeys = true;

  for (auto const& kv : sourceMap) {
    auto res = m_map.insert(kv);

    if (!res.second) {
      noCommonKeys = false;
      if (overwrite) {
        res.first->second = kv.second;
      }
    }
  }
  return noCommonKeys;
}

Json& JsonObject::operator[](String const& key) {
  auto handle = StringInterner::instance().intern(key);
  return m_map[handle];
}

Json const& JsonObject::operator[](String const& key) const {
  auto handle = StringInterner::instance().intern(key);
  // at() or find() for safe lookup
  auto it = m_map.find(handle);
  if (it == m_map.end())
    throw StarException(strf("JsonObject: key '%s' not found", key));
  return it->second;
}

bool JsonObject::operator==(JsonObject const& rhs) const {
  if (m_map.size() != rhs.m_map.size())
    return false;

  // For each (internedKey, value) in 'this' map, see if 'rhs' has the same textual key + value
  for (auto const& kv : m_map) {
    String realKey = StringInterner::instance().lookup(kv.first.id);

    // find the corresponding key in rhs
    auto handle = StringInterner::instance().intern(realKey);
    auto it = rhs.m_map.find(handle);
    if (it == rhs.m_map.end())
      return false;

    // compare the values
    if (kv.second != it->second)
      return false;
  }
  return true;
}

bool JsonObject::operator!=(JsonObject const& rhs) const {
  return !(*this == rhs);
}

// Private implementation of JsonObjectConstIterator
struct JsonObjectConstIterator::Impl {
    typename JsonObject::InternalMap::const_iterator baseIterator;
    mutable std::pair<String, Json> cachedPair;
    
    explicit Impl(typename JsonObject::InternalMap::const_iterator it) 
        : baseIterator(it) {}
};

JsonObjectConstIterator::JsonObjectConstIterator() 
    : m_impl(new Impl(typename JsonObject::InternalMap::const_iterator{})) {}

JsonObjectConstIterator::~JsonObjectConstIterator() {
    delete m_impl;
}

JsonObjectConstIterator::JsonObjectConstIterator(JsonObjectConstIterator const& other)
    : m_impl(new Impl(other.m_impl->baseIterator)) {}

JsonObjectConstIterator& JsonObjectConstIterator::operator=(JsonObjectConstIterator const& other) {
    if (this != &other) {
        delete m_impl;
        m_impl = new Impl(other.m_impl->baseIterator);
    }
    return *this;
}

JsonObjectConstIterator::JsonObjectConstIterator(JsonObjectConstIterator&& other) noexcept
    : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

JsonObjectConstIterator& JsonObjectConstIterator::operator=(JsonObjectConstIterator&& other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

JsonObjectConstIterator::JsonObjectConstIterator(void* baseIterator)
    : m_impl(new Impl(*static_cast<typename JsonObject::InternalMap::const_iterator*>(baseIterator))) {}


JsonObjectConstIterator& JsonObjectConstIterator::operator++() {
    ++m_impl->baseIterator;
    return *this;
}

JsonObjectConstIterator JsonObjectConstIterator::operator++(int) {
    JsonObjectConstIterator temp(*this);
    ++(*this);
    return temp;
}

bool JsonObjectConstIterator::operator==(JsonObjectConstIterator const& rhs) const {
    return m_impl->baseIterator == rhs.m_impl->baseIterator;
}

bool JsonObjectConstIterator::operator!=(JsonObjectConstIterator const& rhs) const {
    return !(*this == rhs);
}

StringInterner::InternedString JsonObjectConstIterator::internedKey() const {
    return m_impl->baseIterator->first;
}

JsonObjectConstIterator::reference JsonObjectConstIterator::operator*() const {
    // We produce a new pair each time.
    m_impl->cachedPair.first = m_impl->baseIterator->first.toString();
    m_impl->cachedPair.second = m_impl->baseIterator->second;
    return m_impl->cachedPair;
}

JsonObjectConstIterator::pointer JsonObjectConstIterator::operator->() const {
    (void)operator*(); // Updates the cached pair
    return &m_impl->cachedPair;
}

Json& JsonObject::set(String const& key, Json const& value) { 
  return (*this)[key] = value; 
}

Json const& JsonObject::get(String const& k) const {
  auto p = ptr(k);
  if (!p) {
    throw StarException(strf("Key '%s' not found in JsonObject::get()", k));
  } else {
    return *p;
  }
}

Json JsonObject::value(String const& k) const {
  return value(k, Json());
}

Json JsonObject::value(String const& k, Json d) const {
  auto p = ptr(k);
  if (!p) {
    return d;
  } else {
    return *p;
  }
}

Maybe<Json> JsonObject::maybe(String const& k) const {
  auto p = ptr(k);
  if (!p) {
    return {};
  } else {
    return *p;
  }
}

Json const* JsonObject::ptr(String const& k) const {
  auto it = find(k); // ephemeral pair
  if (it == end()) {
    return nullptr;
  } else {
    // return the base Json object that is not ephemeral
    auto handle = StringInterner::instance().intern(k);
    auto baseIt = m_map.find(handle);
    return (baseIt == m_map.end()) ? nullptr : &baseIt->second;
  }
}

JsonObjectConstIterator JsonObject::begin() const {
  auto it = m_map.begin();
  return JsonObjectConstIterator(static_cast<void*>(&it));
}

JsonObjectConstIterator JsonObject::end() const {
  auto it = m_map.end();
  return JsonObjectConstIterator(static_cast<void*>(&it));
}

JsonObjectConstIterator JsonObject::find(String const& k) const {
  auto handle = StringInterner::instance().intern(k);
  auto it = m_map.find(handle);
  if (it == m_map.end())
    return end();
  return JsonObjectConstIterator(static_cast<void*>(&it));
}

bool JsonObject::erase(const_iterator it) {
  return m_map.erase(it.internedKey());
}

}
