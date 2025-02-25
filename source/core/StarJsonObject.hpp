#pragma once

#include "StarFlatHashTable.hpp"
#include "StarIntern.hpp"
#include "StarString.hpp"
#include "StarLogging.hpp"
#include "StarDataStream.hpp"

namespace Star {

// Forward declarations
class Json;
STAR_CLASS(JsonObject);

struct InternedKeyHash {
  size_t operator()(StringInterner::InternedString const& ik) const {
    // Just hash by the underlying uint32_t
    return std::hash<uint32_t>()(ik.id);
  }
};

class JsonObjectConstIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<String, Json>;
    using pointer = value_type*;
    using reference = value_type;

    JsonObjectConstIterator();
    ~JsonObjectConstIterator();
    
    // No copy constructor or assignment as it would require Json definition
    JsonObjectConstIterator(JsonObjectConstIterator const& other);
    JsonObjectConstIterator& operator=(JsonObjectConstIterator const& other);
    
    // Move semantics
    JsonObjectConstIterator(JsonObjectConstIterator&& other) noexcept;
    JsonObjectConstIterator& operator=(JsonObjectConstIterator&& other) noexcept;
    
    JsonObjectConstIterator& operator++();
    JsonObjectConstIterator operator++(int);
    
    bool operator==(JsonObjectConstIterator const& rhs) const;
    bool operator!=(JsonObjectConstIterator const& rhs) const;
    
    // These will be implemented in the .cpp file where Json is fully defined
    reference operator*() const;
    pointer operator->() const;

    // This is used by JsonObject::erase
    StringInterner::InternedString internedKey() const;

private:
    friend class JsonObject;
    
    // Implementation details are hidden
    struct Impl;
    Impl* m_impl;
    
    // Private constructor used by JsonObject.  baseIterator is an InternalMap::iterator
    explicit JsonObjectConstIterator(void* baseIterator);
};

class JsonObject {
public:
  // pretend to be a map
  using key_type = String;
  using mapped_type = Json;
  using InternalMap = HashMap<StringInterner::InternedString, Json, InternedKeyHash>;
  using const_iterator = JsonObjectConstIterator;

  pair<JsonObjectConstIterator, bool> insert(pair<String, Json> const& p);
  pair<JsonObjectConstIterator, bool> insert(String const& k, Json const& v);

  JsonObject() = default;
  JsonObject(std::initializer_list<std::pair<String, Json>> init);
  JsonObject(StringMap<Json> const& map);
  
  StringMap<Json> toStringMap();

  size_t size() const;
  void reserve(size_t capacity);
  void clear();
  bool empty() const;
  bool remove(String const& key);
  pair<JsonObjectConstIterator, bool> add(String const& key, Json const& value);
  List<pair<String, Json>> pairs();
  bool erase(String const& key);
  pair<String, Json> first() const;
  List<String> keys() const;
  List<Json> values() const;
  bool contains(String const& key) const;

  bool merge(StringMap<Json> const& sourceMap, bool overwrite = false);
  bool merge(JsonObject const& sourceMap, bool overwrite = false);

  Json& operator[](String const& key);
  Json const& operator[](String const& key) const;

  bool operator==(JsonObject const& rhs) const;
  bool operator!=(JsonObject const& rhs) const;

  JsonObjectConstIterator begin() const;
  JsonObjectConstIterator end() const;

  JsonObjectConstIterator find(String const& k) const;

  Json& set(String const& key, Json const& value);

  Json const& get(String const& k) const;

  Json value(String const& k, Json d) const;
  Json value(String const& k) const;

  Maybe<Json> maybe(String const& k) const;

  bool erase(JsonObjectConstIterator it);
  
  Json const* ptr(String const& k) const;
 
private:
  // This whole class is just a view of the actual InternedString -> Json
  InternalMap m_map;
};

}

template <> struct fmt::formatter<Star::JsonObject> : ostream_formatter {};
