#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif



uniform sampler2D tex_y;
uniform float alpha;
uniform float tex_format;
uniform float imageWidth;

varying vec2 textureOut;

void main() {
    vec4 uyvy = texture2D( tex_y, textureOut);
    float u;
    float v;
    float y1;
    float y2;
    float y;
    if(tex_format == 5){ //UYUV
        u = uyvy[0] - 0.5;
        v = uyvy[2] - 0.5;
        y1 = uyvy[1];
        y2 = uyvy[3];
    }else if(tex_format == 4){ //YUYV
        y1 = uyvy[0];
        u = uyvy[1] - 0.5;
        y2 = uyvy[2];
        v = uyvy[3] - 0.5;
    }

    if (fract(floor(textureOut.x * imageWidth + 0.5) / 2.0) > 0.0)
        y = y2 - 0.0625;       // odd so choose Y1
    else
        y = y1 - 0.0625;       // even so choose Y0

    gl_FragColor.r = y + 1.596 * v;
    gl_FragColor.g = y - 0.813 * v - 0.391 * u;
    gl_FragColor.b = y + 2.018 * u;
    gl_FragColor.a = alpha;
}
