#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;
uniform float tex_format;
uniform float alpha;
uniform float tex_offset;

varying vec2 textureOut;
void main()
{
    vec3 yuv;
    if(tex_format == 0 || tex_format == 1){
        if(tex_format == 0){
            yuv.x = texture2D(tex_y, textureOut - tex_offset).x - 0.0625;
        }else{
            yuv.x = texture2D(tex_y, textureOut - tex_offset).x;
        }
        yuv.y = texture2D(tex_u, textureOut - tex_offset).x - 0.5;
        yuv.z = texture2D(tex_v, textureOut - tex_offset).x - 0.5;
    }else if(tex_format == 2){ // rgb

    }else if(tex_format == 3){ // gray8
        yuv.x = texture2D(tex_y, textureOut - tex_offset).x;
    }


    vec4 rgba;
    if(tex_format == 0){//yuv
        rgba.r = yuv.x + 1.596 * yuv.z;
        rgba.g = yuv.x - 0.813 * yuv.z - 0.391 * yuv.y;
        rgba.b = yuv.x + 2.018 * yuv.y;
    }else if(tex_format == 1){ //yuy-jpeg
        rgba.r = yuv.x + 1.402 * yuv.z;
        rgba.g = yuv.x - 0.34413 * yuv.y - 0.71414 * yuv.z;
        rgba.b = yuv.x + 1.772 * yuv.y;
    }
    else if(tex_format == 2){ //rgb

    }else if(tex_format == 3){ //gray8
        rgba.r = yuv.x;
        rgba.g = yuv.x;
        rgba.b = yuv.x;
    }
    rgba.a = alpha;
    gl_FragColor = rgba;
}
