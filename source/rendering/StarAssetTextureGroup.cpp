#include "StarAssetTextureGroup.hpp"
#include "StarIterator.hpp"
#include "StarTime.hpp"
#include "StarRoot.hpp"
#include "StarAssets.hpp"
#include "StarImageMetadataDatabase.hpp"
#include "StarXXHash.hpp"
#include <cstddef>
#include <cstdint>

namespace Star {

bool AssetTextureHashKey::operator==(AssetTextureHashKey const& other) const {
  return dimensions == other.dimensions && contentHash == other.contentHash;
}

size_t AssetTextureHashKey::hash() const {
  return hashOf(dimensions, contentHash);
}

AssetTextureGroup::AssetTextureGroup(TextureGroupPtr textureGroup)
  : m_textureGroup(std::move(textureGroup)) {
  m_reloadTracker = make_shared<TrackerListener>();
  Root::singleton().registerReloadListener(m_reloadTracker);
}

AssetTextureHashKey AssetTextureGroup::createAssetTextureHashKey(ImageConstPtr const& image) const {
  AssetTextureHashKey key;
  key.dimensions = Vec2U(image->width(), image->height());
  size_t dataSize = image->width() * image->height() * image->bytesPerPixel();
  key.contentHash = xxHash3(reinterpret_cast<char const*>(image->data()), dataSize);

  return key;
}

TexturePtr AssetTextureGroup::loadTexture(AssetPath const& imagePath) {
  return loadTexture(imagePath, false);
}

TexturePtr AssetTextureGroup::tryTexture(AssetPath const& imagePath) {
  return loadTexture(imagePath, true);
}

bool AssetTextureGroup::textureLoaded(AssetPath const& imagePath) const {
  return m_textureMap.contains(imagePath);
}

size_t AssetTextureGroup::cleanup(int64_t textureTimeout) {
  size_t numActions=0;
  if (m_reloadTracker->pullTriggered()) {
    numActions=m_textureMap.size();
    m_textureMap.clear();
    m_textureDeduplicationMap.clear();
  } else {
    int64_t time = Time::monotonicMilliseconds();

    // First collect textures that are still in use (not timed out)
    HashSet<TexturePtr> liveTextures;
    eraseWhere(m_textureMap, [&](auto const& pair) {
      if (time - pair.second.second < textureTimeout) {
        liveTextures.add(pair.second.first);
        return false;  // Keep this entry
      }
      numActions++;
      return true;  // Remove this entry (timed out)
    });
    
    // Then clean up the deduplication map, removing textures that aren't live
    eraseWhere(m_textureDeduplicationMap, [&](auto const& pair) {
      return !liveTextures.contains(pair.second);
    });
  }
  return numActions;
}

TexturePtr AssetTextureGroup::loadTexture(AssetPath const& imagePath, bool tryTexture) {
  // First check if we've already loaded this asset path
  if (auto p = m_textureMap.ptr(imagePath)) {
    // Update the timestamp and return the cached texture
    p->second = Time::monotonicMilliseconds();
    return p->first;
  }

  auto assets = Root::singleton().assets();

  ImageConstPtr image;
  if (tryTexture)
    image = assets->tryImage(imagePath);
  else
    image = assets->image(imagePath);

  if (!image)
    return {};
    
  // Create a hash key from the image for deduplication
  auto hashKey = createAssetTextureHashKey(image);

  // Check if we already have a texture for this image hash
  if (auto existingTexture = m_textureDeduplicationMap.value(hashKey)) {
    // Check if the texture is expired before reusing it
    if (!existingTexture->isExpired()) {
      // Texture is still valid, store in the texture map with current timestamp
      m_textureMap.add(imagePath, {existingTexture, Time::monotonicMilliseconds()});
      return existingTexture;
    } else {
      // Texture is expired, remove it from deduplication map
      m_textureDeduplicationMap.erase(hashKey);
      // Fall through to create a new texture
    }
  }
  
  // This is a key method.  In the opengl implementation (currently the only one we have)
  // this will take the image and blit it into the texture atlas.  After this the actual
  // ImageConstPtr gets cleaned up so that we no longer have the actual pixels as we
  // only refer to it by it's TextureHandle.  If, later, in the optimization phase
  // we need to access the image pixels we have to reload it from the assets system.
  // This was done to save memory usage.  Before we'd retain all image pixels in all the
  // groups so that we could optimize the asset packing, but that was wasteful for ram
  // usage because it stored the image twice, once in the atlas and once standalone.
  auto texture = m_textureGroup->create(*image);

  // add the texture to both maps
  m_textureMap.add(imagePath, {texture, Time::monotonicMilliseconds()});
  m_textureDeduplicationMap.add(hashKey, texture);

  // Do a one-time flush of the asset because it's now stored in the atlas
  // Note that this doesn't flush parent images if this is a subframe or a directive-applied
  // image.  For that we'll just have to wait for it to timeout in the asset cache.
  // That's ok because if we're animating something we might want the parent sprite sheet
  // to still be cached.
  assets->remove(Assets::AssetId{Assets::AssetType::Image, imagePath});
  
  return texture;
}

void AssetTextureGroup::optimalPackingSort(List<pair<AssetPath, Vec2U>>& textures) {
  // Hybrid sorting approach for optimal 2D bin packing
  
  // Define aspect ratio classes as an enum for clarity
  enum class AspectClass {
    Tall,     // taller than wide (portrait)
    Square,   // roughly square
    Wide      // wider than tall (landscape)
  };
  
  // First separate textures into groups by aspect ratio class
  Map<AspectClass, List<pair<AssetPath, Vec2U>>> aspectGroups;
  
  for (auto& texture : textures) {
    const Vec2U& size = texture.second;
    float aspect = float(size[0]) / float(size[1]);
    
    // Classify by aspect ratio
    AspectClass aspectClass;
    if (aspect < 0.75)
      aspectClass = AspectClass::Tall;
    else if (aspect > 1.33)
      aspectClass = AspectClass::Wide;
    else
      aspectClass = AspectClass::Square;
    
    // Add to appropriate group
    aspectGroups[aspectClass].append(texture);
  }
  
  // Clear original list since we'll rebuild it
  textures.clear();
  
  // Process each aspect ratio group with specialized sorting
  for (auto& group : aspectGroups) {
    AspectClass aspectClass = group.first;
    List<pair<AssetPath, Vec2U>>& groupTextures = group.second;
    
    if (aspectClass == AspectClass::Tall) {
      // Tall textures: sort by height (tallest first), then by width
      sort(groupTextures, [](const pair<AssetPath, Vec2U>& a, const pair<AssetPath, Vec2U>& b) {
        const Vec2U& sizeA = a.second;
        const Vec2U& sizeB = b.second;
        if (sizeA[1] != sizeB[1])
          return sizeA[1] > sizeB[1];
        return sizeA[0] > sizeB[0];
      });
    }
    else if (aspectClass == AspectClass::Wide) {
      // Wide textures: sort by width (widest first), then by height
      sort(groupTextures, [](const pair<AssetPath, Vec2U>& a, const pair<AssetPath, Vec2U>& b) {
        const Vec2U& sizeA = a.second;
        const Vec2U& sizeB = b.second;
        if (sizeA[0] != sizeB[0])
          return sizeA[0] > sizeB[0];
        return sizeA[1] > sizeB[1];
      });
    }
    else { // AspectClass::Square
      // Square-ish textures: sort by max dimension, then by area
      sort(groupTextures, [](const pair<AssetPath, Vec2U>& a, const pair<AssetPath, Vec2U>& b) {
        const Vec2U& sizeA = a.second;
        const Vec2U& sizeB = b.second;
        int maxA = std::max(sizeA[0], sizeA[1]);
        int maxB = std::max(sizeB[0], sizeB[1]);
        if (maxA != maxB)
          return maxA > maxB;
        return (sizeA[0] * sizeA[1]) > (sizeB[0] * sizeB[1]);
      });
    }
    
    // Add group textures to main list
    for (auto& texture : groupTextures)
      textures.append(std::move(texture));
  }
}

void AssetTextureGroup::optimizeAtlasesSafely(RendererPtr renderer, int64_t textureTimeout) {

  size_t numCleaned = cleanup(textureTimeout);  // this will remove any timed-out textures
  if (numCleaned == 0 && (m_textureMap.empty() || m_textureGroup->isCompressed())) {
    // Seems optimized already
    return;
  }

  // Make sure all pending GPU operations are complete
  renderer->flush();

  // Next we will destroy all the atlases and rebuild them better than before
  // The idea here is that we're possibly in a state with some compressed atlases and
  // other RGB ones with bad box packing, so we dump everything out, sort it
  // to pack well, and load it all back in.  Once we're done with that we ASTC
  // compress the atlases.
  
  m_textureGroup->reset(); // this removes every atlas and all images in it..
  if (m_textureMap.empty()) {
    // short circuit the null case, but if we're here it means we timed this optimization pass poorly
    Logger::info("Bailing from optimizing an empty texture group.  Look into better synchronization.");
    return;
  }
  
  // Get the list of all image assets
  List<pair<AssetPath, Vec2U>> texturesToPack;
  texturesToPack.reserve(m_textureMap.size());
  for (auto const& pair : m_textureMap) {
    texturesToPack.append({pair.first, pair.second.first->size()});
  }
  
  // From the ashes a new texture map will be reborn
  m_textureMap.clear();
  m_textureDeduplicationMap.clear();
  
  // Sort this list with fancy algorithms to pack nicely.  Pants and underwear first, then shirts and ties.. etc
  optimalPackingSort(texturesToPack);
  
  // now load it all back in in optimal order
  for (auto const& pack : texturesToPack) {
    // note that this texture image is just dropped on the floor to be deleted.
    // if it ends up actually being loaded (and isnt a duplicate or something) then
    // loadTexture will blit it into the atlas so that we don't really need the
    // asset image anymore
    auto texture = loadTexture(pack.first);
    if (!texture) {
      Logger::error("AssetTextureGroup: Failed to load texture: {}", pack.first.basePath);
    }
  }
  
  // Finally compress the atlas image
  renderer->compressTextureGroupSafely(m_textureGroup);
}

void AssetTextureGroup::stats() {
  Logger::info("AssetTextureGroup: {} textures in deduplication map", m_textureDeduplicationMap.size());
  Logger::info("AssetTextureGroup: {} textures in textureMap", m_textureMap.size());
}

}
