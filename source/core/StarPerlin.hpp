#pragma once

#include "StarJson.hpp"
#include "StarBiMap.hpp"
#include "StarInterpolation.hpp"
#include "StarRandom.hpp"
#include "StarMap.hpp"
#include "StarThread.hpp"

namespace Star {

STAR_EXCEPTION(PerlinException, StarException);

enum class PerlinType {
  Uninitialized,
  Perlin,
  Billow,
  RidgedMulti
};
extern EnumMap<PerlinType> const PerlinTypeNames;

// Default sample size for full-quality noise
// For visual effects, a smaller size can be used (e.g. 128 or 256)
template <typename Float>
struct PerlinTables {
  std::shared_ptr<int[]> p;
  std::shared_ptr<Float[]> g1;
  std::shared_ptr<Float[][2]> g2;
  std::shared_ptr<Float[][3]> g3;
  
  static void initialize(uint64_t seed, int sampleSize,
      std::shared_ptr<int[]>& p, 
      std::shared_ptr<Float[]>& g1,
      std::shared_ptr<Float[][2]>& g2,
      std::shared_ptr<Float[][3]>& g3);
};

// Static cache of tables by seed - protected by mutex to ensure thread safety
template <typename Float>
class PerlinTableCache {
public:
  static std::shared_ptr<PerlinTables<Float>> getOrCreate(uint64_t seed, int sampleSize) {
    MutexLocker locker(mutex());
    
    auto key = std::make_pair(seed, sampleSize);
    auto& tablePtr = cache()[key];
    
    if (!tablePtr) {
      tablePtr = std::make_shared<PerlinTables<Float>>();
      PerlinTables<Float>::initialize(seed, sampleSize, 
          tablePtr->p, tablePtr->g1, tablePtr->g2, tablePtr->g3);
    }
    
    return tablePtr;
  }
  
private:
  static Mutex& mutex() {
    static Mutex m;
    return m;
  }
  
  static Map<std::pair<uint64_t, int>, std::shared_ptr<PerlinTables<Float>>>& cache() {
    static Map<std::pair<uint64_t, int>, std::shared_ptr<PerlinTables<Float>>> c;
    return c;
  }
};

template <typename Float, int SampleSize = 512>
class Perlin {
public:
  // Default constructed perlin noise is uninitialized and cannot be queried.
  Perlin();

  Perlin(unsigned octaves, Float freq, Float amp, Float bias, Float alpha, Float beta, uint64_t seed);
  Perlin(PerlinType type, unsigned octaves, Float freq, Float amp, Float bias, Float alpha, Float beta, uint64_t seed);
  Perlin(Json const& config, uint64_t seed);
  explicit Perlin(Json const& json);

  Perlin(Perlin const& perlin);
  Perlin(Perlin&& perlin);

  Perlin& operator=(Perlin const& perlin);
  Perlin& operator=(Perlin&& perlin);

  Float get(Float x) const;
  Float get(Float x, Float y) const;
  Float get(Float x, Float y, Float z) const;

  PerlinType type() const;

  unsigned octaves() const;
  Float frequency() const;
  Float amplitude() const;
  Float bias() const;
  Float alpha() const;
  Float beta() const;

  Json toJson() const;

private:
  static Float s_curve(Float t);
  static void setup(Float v, int& b0, int& b1, Float& r0, Float& r1, int sampleSize);

  static Float at2(Float* q, Float rx, Float ry);
  static Float at3(Float* q, Float rx, Float ry, Float rz);

  Float noise1(Float arg) const;
  Float noise2(Float vec[2]) const;
  Float noise3(Float vec[3]) const;

  void normalize2(Float v[2]) const;
  void normalize3(Float v[3]) const;

  void init(uint64_t seed);

  Float perlin(Float x) const;
  Float perlin(Float x, Float y) const;
  Float perlin(Float x, Float y, Float z) const;

  Float ridgedMulti(Float x) const;
  Float ridgedMulti(Float x, Float y) const;
  Float ridgedMulti(Float x, Float y, Float z) const;

  Float billow(Float x) const;
  Float billow(Float x, Float y) const;
  Float billow(Float x, Float y, Float z) const;

  PerlinType m_type;
  uint64_t m_seed;

  int m_octaves;
  Float m_frequency;
  Float m_amplitude;
  Float m_bias;
  Float m_alpha;
  Float m_beta;

  // Only used for RidgedMulti
  Float m_offset;
  Float m_gain;

  // Shared tables for improved memory efficiency
  std::shared_ptr<PerlinTables<Float>> m_tables;
  
  // References to the tables (for convenience/performance)
  std::shared_ptr<int[]> p;
  std::shared_ptr<Float[]> g1;
  std::shared_ptr<Float[][2]> g2;
  std::shared_ptr<Float[][3]> g3;
  
  // Sample size - fixed at compile time for each instantiation
  const int m_sampleSize = SampleSize;
};


// Simple deterministic float generator that functions as a drop-in replacement for Perlin
template <typename Float>
class DeterministicFloatGenerator {
public:
  DeterministicFloatGenerator() {
    m_type = PerlinType::Perlin;
    m_seed = 0;
    m_octaves = 0;
    m_frequency = 0;
    m_amplitude = 0;
    m_bias = 0;
    m_alpha = 0;
    m_beta = 0;
  }

  DeterministicFloatGenerator(unsigned octaves, Float freq, Float amp, Float bias, Float alpha, Float beta, uint64_t seed) {
    m_type = PerlinType::Perlin;
    m_seed = seed;
    m_octaves = octaves;
    m_frequency = freq;
    m_amplitude = amp;
    m_bias = bias;
    m_alpha = alpha;
    m_beta = beta;
  }

  DeterministicFloatGenerator(PerlinType type, unsigned octaves, Float freq, Float amp, Float bias, Float alpha, Float beta, uint64_t seed) {
    m_type = type;
    m_seed = seed;
    m_octaves = octaves;
    m_frequency = freq;
    m_amplitude = amp;
    m_bias = bias;
    m_alpha = alpha;
    m_beta = beta;
  }

  DeterministicFloatGenerator(Json const& config, uint64_t seed) 
    : DeterministicFloatGenerator(config.set("seed", seed)) {}

  explicit DeterministicFloatGenerator(Json const& json) {
    m_seed = json.getUInt("seed");
    m_octaves = json.getInt("octaves", 1);
    m_frequency = json.getDouble("frequency", 1.0);
    m_amplitude = json.getDouble("amplitude", 1.0);
    m_bias = json.getDouble("bias", 0.0);
    m_alpha = json.getDouble("alpha", 2.0);
    m_beta = json.getDouble("beta", 2.0);
    m_type = PerlinTypeNames.getLeft(json.getString("type", "perlin"));
  }

  DeterministicFloatGenerator(DeterministicFloatGenerator const& other) {
    *this = other;
  }

  DeterministicFloatGenerator(DeterministicFloatGenerator&& other) {
    *this = std::move(other);
  }

  DeterministicFloatGenerator& operator=(DeterministicFloatGenerator const& other) {
    if (this != &other) {
      m_type = other.m_type;
      m_seed = other.m_seed;
      m_octaves = other.m_octaves;
      m_frequency = other.m_frequency;
      m_amplitude = other.m_amplitude;
      m_bias = other.m_bias;
      m_alpha = other.m_alpha;
      m_beta = other.m_beta;
    }
    return *this;
  }

  DeterministicFloatGenerator& operator=(DeterministicFloatGenerator&& other) {
    m_type = other.m_type;
    m_seed = other.m_seed;
    m_octaves = other.m_octaves;
    m_frequency = other.m_frequency;
    m_amplitude = other.m_amplitude;
    m_bias = other.m_bias;
    m_alpha = other.m_alpha;
    m_beta = other.m_beta;
    return *this;
  }

  // Core functionality: hash-based deterministic value generation with smoothing
  Float get(Float x) const {
    Float value = 0;
    Float scale = 1.0;
    
    x *= m_frequency;
    
    for (unsigned i = 0; i < m_octaves; ++i) {
      // Get integer grid points
      int x0 = floor(x);
      int x1 = x0 + 1;
      
      // Get fractional part for interpolation
      Float fx = x - x0;
      
      // Sample at grid points
      Float n0 = hashFloat(computeHash(x0));
      Float n1 = hashFloat(computeHash(x1));
      
      // Apply smooth interpolation
      Float sx = s_curve(fx);
      Float interpolated = (1.0 - sx) * n0 + sx * n1;
      
      value += interpolated / scale;
      scale *= m_alpha;
      x *= m_beta;
    }
    
    return value * m_amplitude + m_bias;
  }

  Float get(Float x, Float y) const {
    Float value = 0;
    Float scale = 1.0;
    
    x *= m_frequency;
    y *= m_frequency;
    
    for (unsigned i = 0; i < m_octaves; ++i) {
      // Get integer grid points
      int x0 = floor(x);
      int x1 = x0 + 1;
      int y0 = floor(y);
      int y1 = y0 + 1;
      
      // Get fractional part for interpolation
      Float fx = x - x0;
      Float fy = y - y0;
      
      // Sample at grid corners
      Float n00 = hashFloat(computeHash(x0, y0));
      Float n10 = hashFloat(computeHash(x1, y0));
      Float n01 = hashFloat(computeHash(x0, y1));
      Float n11 = hashFloat(computeHash(x1, y1));
      
      // Apply smooth interpolation (bilinear with s-curve)
      Float sx = s_curve(fx);
      Float sy = s_curve(fy);
      
      // Interpolate along x for both y values
      Float nx0 = (1.0 - sx) * n00 + sx * n10;
      Float nx1 = (1.0 - sx) * n01 + sx * n11;
      
      // Interpolate along y
      Float interpolated = (1.0 - sy) * nx0 + sy * nx1;
      
      value += interpolated / scale;
      scale *= m_alpha;
      x *= m_beta;
      y *= m_beta;
    }
    
    return value * m_amplitude + m_bias;
  }

  Float get(Float x, Float y, Float z) const {
    Float value = 0;
    Float scale = 1.0;
    
    x *= m_frequency;
    y *= m_frequency;
    z *= m_frequency;
    
    for (unsigned i = 0; i < m_octaves; ++i) {
      // Get integer grid points
      int x0 = floor(x);
      int x1 = x0 + 1;
      int y0 = floor(y);
      int y1 = y0 + 1;
      int z0 = floor(z);
      int z1 = z0 + 1;
      
      // Get fractional part for interpolation
      Float fx = x - x0;
      Float fy = y - y0;
      Float fz = z - z0;
      
      // Sample at grid corners
      Float n000 = hashFloat(computeHash(x0, y0, z0));
      Float n100 = hashFloat(computeHash(x1, y0, z0));
      Float n010 = hashFloat(computeHash(x0, y1, z0));
      Float n110 = hashFloat(computeHash(x1, y1, z0));
      Float n001 = hashFloat(computeHash(x0, y0, z1));
      Float n101 = hashFloat(computeHash(x1, y0, z1));
      Float n011 = hashFloat(computeHash(x0, y1, z1));
      Float n111 = hashFloat(computeHash(x1, y1, z1));
      
      // Apply smooth interpolation (trilinear with s-curve)
      Float sx = s_curve(fx);
      Float sy = s_curve(fy);
      Float sz = s_curve(fz);
      
      // Interpolate along x for each y,z combination
      Float nx00 = (1.0 - sx) * n000 + sx * n100;
      Float nx10 = (1.0 - sx) * n010 + sx * n110;
      Float nx01 = (1.0 - sx) * n001 + sx * n101;
      Float nx11 = (1.0 - sx) * n011 + sx * n111;
      
      // Interpolate along y
      Float nxy0 = (1.0 - sy) * nx00 + sy * nx10;
      Float nxy1 = (1.0 - sy) * nx01 + sy * nx11;
      
      // Interpolate along z
      Float interpolated = (1.0 - sz) * nxy0 + sz * nxy1;
      
      value += interpolated / scale;
      scale *= m_alpha;
      x *= m_beta;
      y *= m_beta;
      z *= m_beta;
    }
    
    return value * m_amplitude + m_bias;
  }

  // Accessor methods to match Perlin API
  PerlinType type() const { return m_type; }
  unsigned octaves() const { return m_octaves; }
  Float frequency() const { return m_frequency; }
  Float amplitude() const { return m_amplitude; }
  Float bias() const { return m_bias; }
  Float alpha() const { return m_alpha; }
  Float beta() const { return m_beta; }

  // Serialization to match Perlin
  Json toJson() const {
    return JsonObject{
      {"seed", m_seed},
      {"octaves", m_octaves},
      {"frequency", m_frequency},
      {"amplitude", m_amplitude},
      {"bias", m_bias},
      {"alpha", m_alpha},
      {"beta", m_beta},
      {"type", PerlinTypeNames.getRight(m_type)}
    };
  }

private:
  PerlinType m_type;
  uint64_t m_seed;
  unsigned m_octaves;
  Float m_frequency;
  Float m_amplitude;
  Float m_bias;
  Float m_alpha;
  Float m_beta;

  // Convert a hash to a Float value in range [-1, 1]
  Float hashFloat(uint64_t hash) const {
    // Use the lower 32 bits for better distribution
    uint32_t value = hash & 0xFFFFFFFF;
    return (Float(value) / Float(0xFFFFFFFF)) * 2.0 - 1.0;
  }
  
  // Helper function for smooth interpolation (cubic Hermite curve)
  static Float s_curve(Float t) {
    return t * t * (3.0 - 2.0 * t);
  }

  // Compute hash for a specific coordinate set
  uint64_t computeHash(Float x, Float y = 0, Float z = 0) const {
    XXHash64 hasher(m_seed);
    
    // Convert floats to integers to avoid precision issues
    int32_t ix = int32_t(x * 1000);
    int32_t iy = int32_t(y * 1000);
    int32_t iz = int32_t(z * 1000);
    
    hasher.push(reinterpret_cast<char const*>(&ix), sizeof(ix));
    hasher.push(reinterpret_cast<char const*>(&iy), sizeof(iy));
    hasher.push(reinterpret_cast<char const*>(&iz), sizeof(iz));
    
    return hasher.digest();
  }
};

// Type aliases to match Perlin
typedef DeterministicFloatGenerator<float> DeterministicFloatF;
typedef DeterministicFloatGenerator<double> DeterministicFloatD;

// Specialized versions with smaller sample sizes for visual effects
template <typename Float>
using VisualPerlin = Perlin<Float, 128>;

typedef Perlin<float> PerlinF;
typedef Perlin<double> PerlinD;
typedef VisualPerlin<float> VisualPerlinF;

template <typename Float>
void PerlinTables<Float>::initialize(uint64_t seed, int sampleSize,
    std::shared_ptr<int[]>& p, 
    std::shared_ptr<Float[]>& g1,
    std::shared_ptr<Float[][2]>& g2,
    std::shared_ptr<Float[][3]>& g3) {
  
  RandomSource randomSource(seed);
  
  p.reset(new int[sampleSize + sampleSize + 2]);
  g1.reset(new Float[sampleSize + sampleSize + 2]);
  g2.reset(new Float[sampleSize + sampleSize + 2][2]);
  g3.reset(new Float[sampleSize + sampleSize + 2][3]);

  int i, j, k;

  for (i = 0; i < sampleSize; i++) {
    p[i] = i;
    g1[i] = (Float)(randomSource.randInt(-sampleSize, sampleSize)) / sampleSize;

    for (j = 0; j < 2; j++)
      g2[i][j] = (Float)(randomSource.randInt(-sampleSize, sampleSize)) / sampleSize;
    // Normalize the vector
    Float s = sqrt(g2[i][0] * g2[i][0] + g2[i][1] * g2[i][1]);
    if (s == 0.0f) {
      g2[i][0] = 1.0f;
      g2[i][1] = 0.0f;
    } else {
      g2[i][0] = g2[i][0] / s;
      g2[i][1] = g2[i][1] / s;
    }

    for (j = 0; j < 3; j++)
      g3[i][j] = (Float)(randomSource.randInt(-sampleSize, sampleSize)) / sampleSize;
    // Normalize the vector
    s = sqrt(g3[i][0] * g3[i][0] + g3[i][1] * g3[i][1] + g3[i][2] * g3[i][2]);
    if (s == 0.0f) {
      g3[i][0] = 1.0f;
      g3[i][1] = 0.0f;
      g3[i][2] = 0.0f;
    } else {
      g3[i][0] = g3[i][0] / s;
      g3[i][1] = g3[i][1] / s;
      g3[i][2] = g3[i][2] / s;
    }
  }

  while (--i) {
    k = p[i];
    p[i] = p[j = randomSource.randUInt(sampleSize - 1)];
    p[j] = k;
  }

  for (i = 0; i < sampleSize + 2; i++) {
    p[sampleSize + i] = p[i];
    g1[sampleSize + i] = g1[i];
    for (j = 0; j < 2; j++)
      g2[sampleSize + i][j] = g2[i][j];
    for (j = 0; j < 3; j++)
      g3[sampleSize + i][j] = g3[i][j];
  }
}

template <typename Float, int SampleSize>
Float Perlin<Float, SampleSize>::s_curve(Float t) {
  return t * t * (3.0 - 2.0 * t);
}

template <typename Float, int SampleSize>
void Perlin<Float, SampleSize>::setup(Float v, int& b0, int& b1, Float& r0, Float& r1, int sampleSize) {
  int iv = floor(v);
  Float fv = v - iv;

  b0 = iv & (sampleSize - 1);
  b1 = (iv + 1) & (sampleSize - 1);
  r0 = fv;
  r1 = fv - 1.0;
}

template <typename Float, int SampleSize>
Float Perlin<Float, SampleSize>::at2(Float* q, Float rx, Float ry) {
  return rx * q[0] + ry * q[1];
}

template <typename Float, int SampleSize>
Float Perlin<Float, SampleSize>::at3(Float* q, Float rx, Float ry, Float rz) {
  return rx * q[0] + ry * q[1] + rz * q[2];
}

template <typename Float, int SampleSize>
Perlin<Float, SampleSize>::Perlin() {
  m_type = PerlinType::Uninitialized;
  m_alpha = 0;
  m_amplitude = 0;
  m_frequency = 0;
  m_seed = 0;
  m_gain = 0;
  m_beta = 0;
  m_offset = 0;
  m_bias = 0;
  m_octaves = 0;
}

template <typename Float, int SampleSize>
Perlin<Float, SampleSize>::Perlin(unsigned octaves, Float freq, Float amp, Float bias, Float alpha, Float beta, uint64_t seed) {
  m_type = PerlinType::Perlin;
  m_seed = seed;

  m_octaves = octaves;
  m_frequency = freq;
  m_amplitude = amp;
  m_bias = bias;
  m_alpha = alpha;
  m_beta = beta;

  // TODO: These ought to be configurable
  m_offset = 1.0;
  m_gain = 2.0;

  init(m_seed);
}

template <typename Float, int SampleSize>
Perlin<Float, SampleSize>::Perlin(PerlinType type, unsigned octaves, Float freq, Float amp, Float bias, Float alpha, Float beta, uint64_t seed) {
  m_type = type;
  m_seed = seed;

  m_octaves = octaves;
  m_frequency = freq;
  m_amplitude = amp;
  m_bias = bias;
  m_alpha = alpha;
  m_beta = beta;

  // TODO: These ought to be configurable
  m_offset = 1.0;
  m_gain = 2.0;

  init(m_seed);
}

template <typename Float, int SampleSize>
Perlin<Float, SampleSize>::Perlin(Json const& config, uint64_t seed)
  : Perlin(config.set("seed", seed)) {}

template <typename Float, int SampleSize>
Perlin<Float, SampleSize>::Perlin(Json const& json) {
  m_seed = json.getUInt("seed");
  m_octaves = json.getInt("octaves", 1);
  m_frequency = json.getDouble("frequency", 1.0);
  m_amplitude = json.getDouble("amplitude", 1.0);
  m_bias = json.getDouble("bias", 0.0);
  m_alpha = json.getDouble("alpha", 2.0);
  m_beta = json.getDouble("beta", 2.0);

  m_offset = json.getDouble("offset", 1.0);
  m_gain = json.getDouble("gain", 2.0);

  m_type = PerlinTypeNames.getLeft(json.getString("type"));

  init(m_seed);
}

template <typename Float, int SampleSize>
Perlin<Float, SampleSize>::Perlin(Perlin const& perlin) {
  *this = perlin;
}

template <typename Float, int SampleSize>
Perlin<Float, SampleSize>::Perlin(Perlin&& perlin) {
  *this = std::move(perlin);
}

template <typename Float, int SampleSize>
Perlin<Float, SampleSize>& Perlin<Float, SampleSize>::operator=(Perlin const& perlin) {
  if (perlin.m_type == PerlinType::Uninitialized) {
    m_type = PerlinType::Uninitialized;
    m_tables = nullptr;
    p = nullptr;
    g3 = nullptr;
    g2 = nullptr;
    g1 = nullptr;
  } else if (this != &perlin) {
    m_type = perlin.m_type;
    m_seed = perlin.m_seed;
    m_octaves = perlin.m_octaves;
    m_frequency = perlin.m_frequency;
    m_amplitude = perlin.m_amplitude;
    m_bias = perlin.m_bias;
    m_alpha = perlin.m_alpha;
    m_beta = perlin.m_beta;
    m_offset = perlin.m_offset;
    m_gain = perlin.m_gain;
    
    // Share the table references
    m_tables = perlin.m_tables;
    p = perlin.p;
    g1 = perlin.g1;
    g2 = perlin.g2;
    g3 = perlin.g3;
  }

  return *this;
}

template <typename Float, int SampleSize>
Perlin<Float, SampleSize>& Perlin<Float, SampleSize>::operator=(Perlin&& perlin) {
  m_type = perlin.m_type;
  m_seed = perlin.m_seed;
  m_octaves = perlin.m_octaves;
  m_frequency = perlin.m_frequency;
  m_amplitude = perlin.m_amplitude;
  m_bias = perlin.m_bias;
  m_alpha = perlin.m_alpha;
  m_beta = perlin.m_beta;
  m_offset = perlin.m_offset;
  m_gain = perlin.m_gain;

  m_tables = std::move(perlin.m_tables);
  p = std::move(perlin.p);
  g3 = std::move(perlin.g3);
  g2 = std::move(perlin.g2);
  g1 = std::move(perlin.g1);

  return *this;
}

template <typename Float, int SampleSize>
void Perlin<Float, SampleSize>::init(uint64_t seed) {
  // Get or create the shared tables for this seed and sample size
  m_tables = PerlinTableCache<Float>::getOrCreate(seed, m_sampleSize);
  
  // Set references to the shared tables
  p = m_tables->p;
  g1 = m_tables->g1;
  g2 = m_tables->g2;
  g3 = m_tables->g3;
}

template <typename Float, int SampleSize>
Float Perlin<Float, SampleSize>::get(Float x) const {
  switch (m_type) {
    case PerlinType::Perlin:
      return perlin(x);
    case PerlinType::Billow:
      return billow(x);
    case PerlinType::RidgedMulti:
      return ridgedMulti(x);
    default:
      throw PerlinException("::get called on uninitialized Perlin");
  }
}

template <typename Float, int SampleSize>
Float Perlin<Float, SampleSize>::get(Float x, Float y) const {
  switch (m_type) {
    case PerlinType::Perlin:
      return perlin(x, y);
    case PerlinType::Billow:
      return billow(x, y);
    case PerlinType::RidgedMulti:
      return ridgedMulti(x, y);
    default:
      throw PerlinException("::get called on uninitialized Perlin");
  }
}

template <typename Float, int SampleSize>
Float Perlin<Float, SampleSize>::get(Float x, Float y, Float z) const {
  switch (m_type) {
    case PerlinType::Perlin:
      return perlin(x, y, z);
    case PerlinType::Billow:
      return billow(x, y, z);
    case PerlinType::RidgedMulti:
      return ridgedMulti(x, y, z);
    default:
      throw PerlinException("::get called on uninitialized Perlin");
  }
}

template <typename Float, int SampleSize>
PerlinType Perlin<Float, SampleSize>::type() const {
  return m_type;
}

template <typename Float, int SampleSize>
unsigned Perlin<Float, SampleSize>::octaves() const {
  return m_octaves;
}

template <typename Float, int SampleSize>
Float Perlin<Float, SampleSize>::frequency() const {
  return m_frequency;
}

template <typename Float, int SampleSize>
Float Perlin<Float, SampleSize>::amplitude() const {
  return m_amplitude;
}

template <typename Float, int SampleSize>
Float Perlin<Float, SampleSize>::bias() const {
  return m_bias;
}

template <typename Float, int SampleSize>
Float Perlin<Float, SampleSize>::alpha() const {
  return m_alpha;
}

template <typename Float, int SampleSize>
Float Perlin<Float, SampleSize>::beta() const {
  return m_beta;
}

template <typename Float, int SampleSize>
Json Perlin<Float, SampleSize>::toJson() const {
  return JsonObject{
    {"seed", m_seed},
    {"octaves", m_octaves},
    {"frequency", m_frequency},
    {"amplitude", m_amplitude},
    {"bias", m_bias},
    {"alpha", m_alpha},
    {"beta", m_beta},
    {"offset", m_offset},
    {"gain", m_gain},
    {"type", PerlinTypeNames.getRight(m_type)}
  };
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::noise1(Float arg) const {
  int bx0, bx1;
  Float rx0, rx1, sx, u, v;

  setup(arg, bx0, bx1, rx0, rx1, m_sampleSize);

  sx = s_curve(rx0);
  u = rx0 * g1[p[bx0]];
  v = rx1 * g1[p[bx1]];

  return (lerp(sx, u, v));
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::noise2(Float vec[2]) const {
  int bx0, bx1, by0, by1, b00, b10, b01, b11;
  Float rx0, rx1, ry0, ry1, sx, sy, a, b, u, v;
  int i, j;

  setup(vec[0], bx0, bx1, rx0, rx1, m_sampleSize);
  setup(vec[1], by0, by1, ry0, ry1, m_sampleSize);

  i = p[bx0];
  j = p[bx1];

  b00 = p[i + by0];
  b10 = p[j + by0];
  b01 = p[i + by1];
  b11 = p[j + by1];

  sx = s_curve(rx0);
  sy = s_curve(ry0);

  u = at2(g2[b00], rx0, ry0);
  v = at2(g2[b10], rx1, ry0);
  a = lerp(sx, u, v);

  u = at2(g2[b01], rx0, ry1);
  v = at2(g2[b11], rx1, ry1);
  b = lerp(sx, u, v);

  return lerp(sy, a, b);
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::noise3(Float vec[3]) const {
  int bx0, bx1, by0, by1, bz0, bz1, b00, b10, b01, b11;
  Float rx0, rx1, ry0, ry1, rz0, rz1, sx, sy, sz, a, b, c, d, u, v;
  int i, j;

  setup(vec[0], bx0, bx1, rx0, rx1, m_sampleSize);
  setup(vec[1], by0, by1, ry0, ry1, m_sampleSize);
  setup(vec[2], bz0, bz1, rz0, rz1, m_sampleSize);

  i = p[bx0];
  j = p[bx1];

  b00 = p[i + by0];
  b10 = p[j + by0];
  b01 = p[i + by1];
  b11 = p[j + by1];

  sx = s_curve(rx0);
  sy = s_curve(ry0);
  sz = s_curve(rz0);

  u = at3(g3[b00 + bz0], rx0, ry0, rz0);
  v = at3(g3[b10 + bz0], rx1, ry0, rz0);
  a = lerp(sx, u, v);

  u = at3(g3[b01 + bz0], rx0, ry1, rz0);
  v = at3(g3[b11 + bz0], rx1, ry1, rz0);
  b = lerp(sx, u, v);

  c = lerp(sy, a, b);

  u = at3(g3[b00 + bz1], rx0, ry0, rz1);
  v = at3(g3[b10 + bz1], rx1, ry0, rz1);
  a = lerp(sx, u, v);

  u = at3(g3[b01 + bz1], rx0, ry1, rz1);
  v = at3(g3[b11 + bz1], rx1, ry1, rz1);
  b = lerp(sx, u, v);

  d = lerp(sy, a, b);

  return lerp(sz, c, d);
}

template <typename Float, int SampleSize>
void Perlin<Float, SampleSize>::normalize2(Float v[2]) const {
  Float s;

  s = sqrt(v[0] * v[0] + v[1] * v[1]);
  if (s == 0.0f) {
    v[0] = 1.0f;
    v[1] = 0.0f;
  } else {
    v[0] = v[0] / s;
    v[1] = v[1] / s;
  }
}

template <typename Float, int SampleSize>
void Perlin<Float, SampleSize>::normalize3(Float v[3]) const {
  Float s;

  s = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (s == 0.0f) {
    v[0] = 1.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
  } else {
    v[0] = v[0] / s;
    v[1] = v[1] / s;
    v[2] = v[2] / s;
  }
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::perlin(Float x) const {
  int i;
  Float val, sum = 0;
  Float p, scale = 1;

  p = x * m_frequency;
  for (i = 0; i < m_octaves; i++) {
    val = noise1(p);
    sum += val / scale;
    scale *= m_alpha;
    p *= m_beta;
  }
  return sum * m_amplitude + m_bias;
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::perlin(Float x, Float y) const {
  int i;
  Float val, sum = 0;
  Float p[2], scale = 1;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  for (i = 0; i < m_octaves; i++) {
    val = noise2(p);
    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
  }
  return sum * m_amplitude + m_bias;
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::perlin(Float x, Float y, Float z) const {
  int i;
  Float val, sum = 0;
  Float p[3], scale = 1;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  p[2] = z * m_frequency;
  for (i = 0; i < m_octaves; i++) {
    val = noise3(p);
    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
    p[2] *= m_beta;
  }

  return sum * m_amplitude + m_bias;
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::ridgedMulti(Float x) const {
  Float val, sum = 0;
  Float scale = 1;
  Float weight = 1.0;

  x *= m_frequency;
  for (int i = 0; i < m_octaves; ++i) {
    val = noise1(x);

    val = m_offset - fabs(val);
    val *= val;
    val *= weight;

    weight = clamp<Float>(val * m_gain, 0.0, 1.0);

    sum += val / scale;
    scale *= m_alpha;
    x *= m_beta;
  }

  return ((sum * 1.25) - 1.0) * m_amplitude + m_bias;
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::ridgedMulti(Float x, Float y) const {
  Float val, sum = 0;
  Float p[2], scale = 1;
  Float weight = 1.0;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  for (int i = 0; i < m_octaves; ++i) {
    val = noise2(p);

    val = m_offset - fabs(val);
    val *= val;
    val *= weight;

    weight = clamp<Float>(val * m_gain, 0.0, 1.0);

    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
  }

  return ((sum * 1.25) - 1.0) * m_amplitude + m_bias;
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::ridgedMulti(Float x, Float y, Float z) const {
  Float val, sum = 0;
  Float p[3], scale = 1;
  Float weight = 1.0;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  p[2] = z * m_frequency;
  for (int i = 0; i < m_octaves; ++i) {
    val = noise3(p);

    val = m_offset - fabs(val);
    val *= val;
    val *= weight;

    weight = clamp<Float>(val * m_gain, 0.0, 1.0);

    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
    p[2] *= m_beta;
  }

  return ((sum * 1.25) - 1.0) * m_amplitude + m_bias;
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::billow(Float x) const {
  Float val, sum = 0;
  Float p, scale = 1;

  p = x * m_frequency;
  for (int i = 0; i < m_octaves; i++) {
    val = noise1(p);
    val = 2.0 * fabs(val) - 1.0;

    sum += val / scale;
    scale *= m_alpha;
    p *= m_beta;
  }
  return (sum + 0.5) * m_amplitude + m_bias;
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::billow(Float x, Float y) const {
  Float val, sum = 0;
  Float p[2], scale = 1;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  for (int i = 0; i < m_octaves; i++) {
    val = noise2(p);
    val = 2.0 * fabs(val) - 1.0;

    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
  }
  return (sum + 0.5) * m_amplitude + m_bias;
}

template <typename Float, int SampleSize>
inline Float Perlin<Float, SampleSize>::billow(Float x, Float y, Float z) const {
  Float val, sum = 0;
  Float p[3], scale = 1;

  p[0] = x * m_frequency;
  p[1] = y * m_frequency;
  p[2] = z * m_frequency;
  for (int i = 0; i < m_octaves; i++) {
    val = noise3(p);
    val = 2.0 * fabs(val) - 1.0;

    sum += val / scale;
    scale *= m_alpha;
    p[0] *= m_beta;
    p[1] *= m_beta;
    p[2] *= m_beta;
  }

  return (sum + 0.5) * m_amplitude + m_bias;
}

}
