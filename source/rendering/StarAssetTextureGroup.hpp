#pragma once

#include "StarMaybe.hpp"
#include "StarString.hpp"
#include "StarBiMap.hpp"
#include "StarListener.hpp"
#include "StarRenderer.hpp"
#include "StarAssetPath.hpp"
#include "StarHash.hpp"
#include <cstddef>

namespace Star {

STAR_CLASS(AssetTextureGroup);

// Forward declaration for specialization
class AssetTextureGroup;

// Represents the key data needed to identify a unique image
struct AssetTextureHashKey {
  Vec2U dimensions;
  uint64_t contentHash;
    
  bool operator==(AssetTextureHashKey const& other) const;
    
  // Use this method to satisfy both hash() and std::hash
  size_t hash() const;
    
  // Allow automatic use with std::hash
  operator size_t() const { return hash(); }
};

template <>
struct hash<AssetTextureHashKey> {
  size_t operator()(AssetTextureHashKey const& key) const {
    return key.hash();
  }
};

// Creates a renderer texture group for textures loaded directly from Assets.
class AssetTextureGroup {
public:
  // Creates a texture group using the given renderer and textureFiltering for
  // the managed textures.
  AssetTextureGroup(TextureGroupPtr textureGroup);

  // Load the given texture into the texture group if it is not loaded, and
  // return the texture pointer.
  TexturePtr loadTexture(AssetPath const& imagePath);

  // If the texture is loaded and ready, returns the texture pointer, otherwise
  // queues the texture using Assets::tryImage and returns nullptr.
  TexturePtr tryTexture(AssetPath const& imagePath);

  // Has the texture been loaded?
  bool textureLoaded(AssetPath const& imagePath) const;

  // Frees textures that haven't been used in more than 'textureTimeout' time.
  // If Root has been reloaded, will simply clear the texture group.
  size_t cleanup(int64_t textureTimeout);

  TextureGroupPtr getTextureGroup() { return m_textureGroup; }

  // Optimize texture atlases with synchronization through the renderer
  void optimizeAtlasesSafely(RendererPtr renderer, int64_t textureTimeout=10000);

  void stats();
  
private:
  
  // Returns the texture parameters.  If tryTexture is true, then returns none
  // if the texture is not loaded, and queues it, otherwise loads texture
  // immediately
  TexturePtr loadTexture(AssetPath const& imagePath, bool tryTexture);
  
  // Creates a hash key from an image for deduplication
  AssetTextureHashKey createAssetTextureHashKey(ImageConstPtr const& image) const;

  // Sorts the list of textures in a fashion where they will pack well into an atlas
  void optimalPackingSort(List<pair<AssetPath, Vec2U>>& textures);

  TextureGroupPtr m_textureGroup;
  HashMap<AssetTextureHashKey, TexturePtr> m_textureDeduplicationMap;
  HashMap<AssetPath, pair<TexturePtr, int64_t>> m_textureMap;
  TrackerListenerPtr m_reloadTracker;
};

}
