#pragma once

#include "StarString.hpp"
#include "StarThread.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <cassert>

namespace Star {

STAR_CLASS(StringInterner);

// This class has three ways of interning strings:
// 1) Numeric strings occupy slots 0-9999 and are implicitly converted
// 2) A small number of very frequent strings are special cased to be fast
// 3) The rest of the intern storage is a big hashmap/vector combo for regular lookup
class StringInterner {
public:

  // Returns a singleton instance
  static StringInterner& instance() {
    static StringInterner s_instance;
    return s_instance;
  }

  // debug stuff
  static String dump() {
    String dump;

    dump.append("FIXED KEYS:\n");
    for (auto k : *instance().m_fixedKeys) {
      dump.append(k + "\n");
    }
    dump.append("\nFIXED KEYMAP\n");
    for (auto kv : *instance().m_fixedKeyMap) {
      dump.append(kv.first + " : " + kv.second + "\n");
    }

    dump.append("\nDYNAMIC KEYS:\n");
    for (auto k : instance().m_dynamicStorage) {
      dump.append(k + "\n");
    }
    dump.append("\nDYNAMIC KEYMAP\n");
    for (auto kv : instance().m_dynamicMap) {
      dump.append(kv.first + " : " + std::to_string(kv.second) + "\n");
    }

    printf("%s\n", dump.utf8Ptr());
    return dump;
  }

  // A lightweight handle to the interned string
  struct InternedString {
    uint16_t id;
    // Comparison operators, etc.
    bool operator==(const InternedString& other) const {
      return id == other.id;
    }

    bool operator<(const InternedString& other) const {
      return this->toString() < other.toString();
    }

    const String toString() const {
      return StringInterner::instance().lookup(*this);
    }

    InternedString() : id(UINT16_MAX) {} // will fail lookup 
    explicit operator bool() const { return id != UINT16_MAX; } // for good measure
    InternedString(uint16_t id) : id(id) {}
    InternedString(std::string const& s) : id(StringInterner::instance().intern(s).id) {}
    InternedString(String const& s) : id(StringInterner::instance().intern(s).id) {}
    InternedString(char const* s) : id(StringInterner::instance().intern(s).id) {}

    // Implicit string constructor???? seems risky
    operator String() const {
      // Every time this conversion happens, we do a lookup. 
      // That can be expensive if it happens frequently.
      return StringInterner::instance().lookup(id);
    }
  };

  InternedString intern(const String& s) {
    // 1. Check if numeric and in range
    int numericValue = parseNumeric(s);
    if (numericValue >= 0 && numericValue <= 9999) {
      return InternedString{ static_cast<uint16_t>(numericValue) };
    }

    // 2. Check if it's one of our fixed special strings
    auto fixedIt = m_fixedKeyMap->find(s);
    if (fixedIt != m_fixedKeyMap->end()) {
      return InternedString{ fixedIt->second };
    }

    // 3. Look up in dynamic map
    {
      ReadLocker readLocker(m_dynamicMutex);
      auto it = m_dynamicMap.find(s.utf8());
      if (it != m_dynamicMap.end()) {
        // Already interned
        return InternedString{ it->second };
      }
    } // there is a race here
    { // unfortunately no way to upgrade the read lock
      WriteLocker writeLocker(m_dynamicMutex);

      // gotta look one more time in case somebody outraced us
      auto it = m_dynamicMap.find(s.utf8());
      if (it != m_dynamicMap.end()) {
        return InternedString{ it->second };
      }

      uint16_t newId = s_dynamicBase + static_cast<uint16_t>(m_dynamicStorage.size());
      m_dynamicStorage.emplace_back(s);       // store in our vector
      m_dynamicMap[s.utf8()] = newId;            // record in our map
      return InternedString{ newId };
    }
  }

  // Retrieves the interned string from an ID
  const String& lookup(InternedString handle) const {
    uint16_t id = handle.id;

    // 1. If it's in numeric range [0..9999]
    if (id <= 9999) {
      return getNumericString(id);
    }

    // 2. If it's in fixed range
    if (id >= s_fixedBase && id < s_fixedBase + m_fixedKeys->size()) {
      return (*m_fixedKeys)[id-s_fixedBase];
    }

    // 3. Otherwise, it's dynamic
    //    offset from s_dynamicBase
    {
      ReadLocker readLocker(m_dynamicMutex);
      uint16_t offset = id - s_dynamicBase;
      if (offset < m_dynamicStorage.size()) {
        return m_dynamicStorage[offset];
      }
    }

    // If we get here, it's invalid or out of range
    throw std::out_of_range("Invalid interned string ID");
  }

private:

  // numeric values are 0-9999 - programmatic conversion here
  // fixed (common) keys are 10000-10999 but only a small subset of this is used
  // the truly dynamic hashtable is 11000-UINT16_MAX
  static const uint16_t s_fixedBase = 10000;  
  static const uint16_t s_dynamicBase = 11000; 
  
  void initializeFixedKeymap(std::vector<String> const& commonStrings) {
    // Create a temporary non-const map for initialization
    auto tempMap = std::make_unique<HashMap<String, InternedString>>(commonStrings.size());
    auto tempKeys = std::make_unique<std::vector<String>>(commonStrings.size());
        
    // Populate it
    uint16_t id = s_fixedBase;
    for (auto const& str : commonStrings) {
      (*tempMap)[str] = InternedString{id};
      (*tempKeys)[id-s_fixedBase] = str;
      id++;
    }
        
    // Atomically publish the new map - this implicitly converts to 
    // unique_ptr<const HashMap<...>> through move assignment
    m_fixedKeyMap = std::move(tempMap);
    m_fixedKeys = std::move(tempKeys);
  }

  // Private constructor for singleton
  StringInterner()
  {
    // The entire assets subsystem has about 14,000 unique keys but a given
    // game run only loads about 10k of them
    m_dynamicMap.reserve(10000);
    m_dynamicStorage.resize(10000);
    
    // Initialize m_fixedKeys in the same order as IDs
    initializeFixedKeymap({
        "rotation", "properties", "id", "visible", "height",
        "width", "y", "x", "name", "type"
      });

    // prepopulate the cache with a range of known ids we'll
    // be using
    for (uint16_t i=0; i<1000; i++) {
      getNumericString(i);
    }
  }

  // Helper to parse string as integer in 0..9999 range
  // Returns -1 if not valid or out of range
  int parseNumeric(const String& s) const {
    // Quick check for empty, leading signs, etc. 
    // (Implementation detail — might want to handle leading zeroes, etc.)
    if (s.empty() || s.size() > 4) {
      return -1;
    }
    int val = 0;
    for (char c : s) {
      if (c < '0' || c > '9') return -1;
      val = val * 10 + (c - '0');
      if (val > 9999) return -1;
    }
    return val;
  }

  // We store numeric strings [0..9999] in a cache for quick ID->string
  // (To save repeated allocations).
  // NOTE - this is not thread-safe but with pre-population it's very unlikely
  // to cause a problem.
  const String& getNumericString(uint16_t val) const {
    assert(val <= 9999);
    // Build the cache on first access
    if (numericStringsCache_.empty()) {
      numericStringsCache_.resize(10000);
      for (int i = 0; i < 10000; i++) {
        numericStringsCache_[i] = std::to_string(i);
      }
    }
    return numericStringsCache_[val];
  }

  // --- Internal Data Members ---

  // For numeric IDs -> string caching
  mutable std::vector<String> numericStringsCache_; // lazy-initialized

  // About 70% of Json fields are the same set of 6 or 7 keys, so we
  // create a fixed keymap that is not wrapped in a mutex allowing
  // fast parallel reads for the general purpose.  Combined with
  // numeric fields, this allows for mutex-free interning in most cases
  std::unique_ptr<const std::vector<String>> m_fixedKeys; // index = ID - s_fixedBase
  std::unique_ptr<const HashMap<String, InternedString>> m_fixedKeyMap;

  // For dynamic strings - aka unwashed masses
  mutable ReadersWriterMutex m_dynamicMutex; // guard the dynamic storage with read/write mutex
  std::vector<String> m_dynamicStorage; // index = ID - s_dynamicBase
  std::unordered_map<std::string, uint16_t> m_dynamicMap;

};

}
