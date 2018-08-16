#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;
uniform float tex_format = 1.0;
uniform float alpha = 1.0;
uniform float tex_offset = 0;
uniform float imageWidth = 0;
uniform float imageHeight = 0;
uniform bool enableHDR = false;

varying vec2 textureOut;

float gamma = 2.2;
vec3 toLinear(in vec3 colour) { return pow(colour, vec3(gamma)); }
vec3 toHDR(in vec3 colour, in float range) { return toLinear(colour) * range; }


void main()
{
    vec3 yuv;
    vec4 rgba;
    if(tex_format == 0 || tex_format == 1){
        if(tex_format == 0){
            yuv.r = texture2D(tex_y, textureOut - tex_offset).r - 0.0625;
        }else{
            yuv.r = texture2D(tex_y, textureOut - tex_offset).r;
        }
        yuv.g = texture2D(tex_u, textureOut - tex_offset).r - 0.5;
        yuv.b = texture2D(tex_v, textureOut - tex_offset).r - 0.5;
    }else if(tex_format == 2){ // rgb
        yuv = texture2D(tex_y, textureOut).rgb;
    }else if(tex_format == 3){ // gray8
        yuv.r = texture2D(tex_y, textureOut - tex_offset).r;
    }else if(tex_format == 6){ //BGR
        yuv = texture2D(tex_y, textureOut).bgr;
    }else if(tex_format == 10){//yuv420p10le yuv444p10le
        vec3 yuv_l;
        vec3 yuv_h;
        yuv_l.x = texture2D(tex_y, textureOut).r;
        yuv_h.x = texture2D(tex_y, textureOut).a;
        yuv_l.y = texture2D(tex_u, textureOut).r;
        yuv_h.y = texture2D(tex_u, textureOut).a;
        yuv_l.z = texture2D(tex_v, textureOut).r;
        yuv_h.z = texture2D(tex_v, textureOut).a;
        yuv = (yuv_l * 255.0 + yuv_h * 255.0 * 256.0) / (1023.0) - vec3(16.0 / 255.0, 0.5, 0.5);
    }



    if(tex_format == 0 || tex_format == 10){//yuv | p10le
        rgba.r = yuv.r + 1.596 * yuv.b;
        rgba.g = yuv.r - 0.813 * yuv.b - 0.391 * yuv.g;
        rgba.b = yuv.r + 2.018 * yuv.g;
    }else if(tex_format == 1){ //yuy-jpeg
        rgba.r = yuv.r + 1.402 * yuv.b;
        rgba.g = yuv.r - 0.34413 * yuv.g - 0.71414 * yuv.b;
        rgba.b = yuv.r + 1.772 * yuv.g;
    }
    else if(tex_format == 2){ //rgb
        rgba.rgb = yuv.rgb;
    }else if(tex_format == 3){ //gray8
        rgba.r = yuv.r;
        rgba.g = yuv.r;
        rgba.b = yuv.r;
    }else if(tex_format == 6){ //BGR
        rgba.r = yuv.b;
        rgba.g = yuv.g;
        rgba.b = yuv.r;
    }
    rgba.a = alpha;
    if(enableHDR){
        rgba.rgb = toHDR(rgba.rgb,1.0);
    }
    rgba.a = alpha;
    gl_FragColor = rgba;
}
