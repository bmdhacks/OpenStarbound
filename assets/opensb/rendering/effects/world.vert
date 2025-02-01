#ifdef GL_ES
precision mediump float;
#endif

// Uniforms
uniform vec2 textureSize0;
uniform vec2 textureSize1;
uniform vec2 textureSize2;
uniform vec2 textureSize3;
uniform vec2 screenSize;
uniform mat3 vertexTransform;
uniform bool vertexRounding;
uniform vec2 lightMapSize;
uniform vec2 lightMapScale;
uniform vec2 lightMapOffset;

// Attributes
attribute vec2 vertexPosition;
attribute vec2 vertexTextureCoordinate;
attribute float vertexTextureIndex;
attribute vec4 vertexColor;
attribute float vertexData; // Converted from int to float due to ES 2.0 restrictions

// Varyings
varying vec2 vFragmentTextureCoordinate;
varying float vFragmentTextureIndex;
varying vec4 vFragmentColor;
varying float vFragmentLightMapMultiplier;
varying vec2 vFragmentLightMapCoordinate;

void main() {
  vec2 screenPosition = (vertexTransform * vec3(vertexPosition, 1.0)).xy;

  if (vertexRounding) {
    // Extract rounding flags from vertexData
    float roundXFlag = mod(floor(vertexData / 8.0), 2.0); // Equivalent to (vertexData >> 3) & 0x1
    float roundYFlag = mod(floor(vertexData / 16.0), 2.0); // Equivalent to (vertexData >> 4) & 0x1
    
    if (roundXFlag > 0.5)
      screenPosition.x = floor(screenPosition.x + 0.5);
    if (roundYFlag > 0.5)
      screenPosition.y = floor(screenPosition.y + 0.5);
  }
  
  // Extract light map multiplier from vertexData
  vFragmentLightMapMultiplier = mod(floor(vertexData / 4.0), 2.0); // Equivalent to (vertexData >> 2) & 0x1

  // Extract texture index from vertexData
  float vertexTextureIndex = mod(vertexData, 4.0); // Equivalent to vertexData & 0x3

  // Compute light map coordinate
  vFragmentLightMapCoordinate = (screenPosition / lightMapScale) - lightMapOffset * lightMapSize / screenSize;

  // Select correct texture size
  if (vertexTextureIndex > 2.9)
    vFragmentTextureCoordinate = vertexTextureCoordinate / textureSize3;
  else if (vertexTextureIndex > 1.9)
    vFragmentTextureCoordinate = vertexTextureCoordinate / textureSize2;
  else if (vertexTextureIndex > 0.9)
    vFragmentTextureCoordinate = vertexTextureCoordinate / textureSize1;
  else
    vFragmentTextureCoordinate = vertexTextureCoordinate / textureSize0;

  vFragmentTextureIndex = vertexTextureIndex;
  vFragmentColor = vertexColor;

  // Convert to clip space
  gl_Position = vec4(screenPosition / screenSize * 2.0 - 1.0, 0.0, 1.0);
}
