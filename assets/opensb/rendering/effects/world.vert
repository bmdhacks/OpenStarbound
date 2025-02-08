#version 100
precision mediump float;

uniform vec2 textureSize0;
uniform vec2 textureSize1;
uniform vec2 textureSize2;
uniform vec2 textureSize3;
uniform vec2 screenSize;
uniform mat3 vertexTransform;
uniform vec2 lightMapSize;
uniform vec2 lightMapScale;
uniform vec2 lightMapOffset;

attribute vec2 vertexPosition;
attribute vec2 vertexTextureCoordinate;
attribute vec4 vertexColor;
attribute float vertexData; // textureIndex (bits 0-1), lightMapMultiplier (bit 2)

varying vec2 fragmentTextureCoordinate;
varying float fragmentTextureIndex;
varying vec4 fragmentColor;
varying float fragmentLightMapMultiplier; 
varying vec2 fragmentLightMapCoordinate;

void main() {
    // Unpack data
  float textureIndex = mod(vertexData, 4.0); // Bits 0-1
  float lightMapMultiplier = step(4.0, vertexData); // Bit 2
  
  vec3 transformed = vertexTransform * vec3(vertexPosition, 1.0);
  vec2 screenPosition = transformed.xy;
  
  fragmentTextureIndex = textureIndex;
  fragmentLightMapMultiplier = lightMapMultiplier;
  fragmentLightMapCoordinate = (screenPosition / lightMapScale) - lightMapOffset * lightMapSize / screenSize;
  
  // Texture coordinate selection
  if (textureIndex > 2.9) {
    fragmentTextureCoordinate = vertexTextureCoordinate / textureSize3;
  } else if (textureIndex > 1.9) {
    fragmentTextureCoordinate = vertexTextureCoordinate / textureSize2;
  } else if (textureIndex > 0.9) {
    fragmentTextureCoordinate = vertexTextureCoordinate / textureSize1;
  } else {
    fragmentTextureCoordinate = vertexTextureCoordinate / textureSize0;
  }
  
  fragmentColor = vertexColor;
  gl_Position = vec4((screenPosition / screenSize) * 2.0 - 1.0, 0.0, 1.0);
}
