#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif



uniform sampler2D tex_y;
uniform float alpha;
uniform float tex_format;

varying vec2 textureOut;

void main() {
    vec3 yuv = texture2D( tex_y, textureOut ).xyz;
    if(tex_format == 0){//yuv
        gl_FragColor.r = yuv.x + 1.596 * yuv.z;
        gl_FragColor.g = yuv.x - 0.813 * yuv.z - 0.391 * yuv.y;
        gl_FragColor.b = yuv.x + 2.018 * yuv.y;
    }else if(tex_format == 1){ //yuy-jpeg
        gl_FragColor.r = yuv.x + 1.402 * yuv.z;
        gl_FragColor.g = yuv.x - 0.34413 * yuv.y - 0.71414 * yuv.z;
        gl_FragColor.b = yuv.x + 1.772 * yuv.y;
    }
    gl_FragColor.a = alpha;
}
