#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

attribute vec4 vertexIn;
attribute vec2 textureIn;
uniform mat4 matrix;
varying vec2 textureOut;
void main(void) {
    gl_Position = matrix * vertexIn;
    textureOut = textureIn;
}
