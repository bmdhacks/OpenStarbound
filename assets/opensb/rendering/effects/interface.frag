#ifdef GL_ES
precision mediump float;
#endif

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;

varying vec2 vFragmentTextureCoordinate;
varying float vFragmentTextureIndex;
varying vec4 vFragmentColor;

void main() {
  vec4 texColor;
  
  if (vFragmentTextureIndex == 3.0)
    texColor = texture2D(texture3, vFragmentTextureCoordinate);
  else if (vFragmentTextureIndex == 2.0)
    texColor = texture2D(texture2, vFragmentTextureCoordinate);
  else if (vFragmentTextureIndex == 1.0)
    texColor = texture2D(texture1, vFragmentTextureCoordinate);
  else
    texColor = texture2D(texture0, vFragmentTextureCoordinate);

  if (texColor.a <= 0.0)
    discard;

  gl_FragColor = texColor * vFragmentColor;
}
