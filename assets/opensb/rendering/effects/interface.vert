#ifdef GL_ES
precision mediump float;
#endif

// Uniforms (same as original)
uniform vec2 textureSize0;
uniform vec2 textureSize1;
uniform vec2 textureSize2;
uniform vec2 textureSize3;
uniform vec2 screenSize;
uniform mat3 vertexTransform;

// Attributes (note: vertexData is now a float)
attribute vec2 aVertexPosition;
attribute vec4 aVertexColor;
attribute vec2 aVertexTextureCoordinate;
attribute float aVertexData;  // originally an int

// Varyings passed to the fragment shader
varying vec2 vFragmentTextureCoordinate;
varying float vFragmentTextureIndex;
varying vec4 vFragmentColor;

void main() {
  // Transform the vertex position.
  vec2 screenPosition = (vertexTransform * vec3(aVertexPosition, 1.0)).xy;
  gl_Position = vec4(screenPosition / screenSize * 2.0 - 1.0, 0.0, 1.0);
  
  // Extract the texture index from the lower two bits of aVertexData.
  // This emulates: int vertexTextureIndex = vertexData & 0x3;
  float texIndex = mod(aVertexData, 4.0);
  
  // Select the proper texture coordinate based on the texture index.
  if (texIndex == 3.0)
    vFragmentTextureCoordinate = aVertexTextureCoordinate / textureSize3;
  else if (texIndex == 2.0)
    vFragmentTextureCoordinate = aVertexTextureCoordinate / textureSize2;
  else if (texIndex == 1.0)
    vFragmentTextureCoordinate = aVertexTextureCoordinate / textureSize1;
  else
    vFragmentTextureCoordinate = aVertexTextureCoordinate / textureSize0;
  
  // Pass the texture index and vertex color to the fragment shader.
  vFragmentTextureIndex = texIndex;
  vFragmentColor = aVertexColor;
}
