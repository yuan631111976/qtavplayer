#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif



uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform float alpha;
uniform float tex_format;
uniform float imageWidth;
uniform float imageHeight;

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
    }else if(tex_format == 7){ //UYYVYY

    }else if(tex_format == 8 || tex_format == 9){ //NV12 | NV21
        //uyvy 只有Y数据
        vec4 uv = texture2D( tex_u, textureOut);
        y = uyvy.r - 0.0625;
        if(tex_format == 9){
            u = uv.a - 0.5;
            v = uv.r - 0.5;
        }else{
            u = uv.r - 0.5;
            v = uv.a - 0.5;
        }
    }

    if(tex_format == 5 || tex_format == 6){ //UYUV | YUYV
        if (fract(floor(textureOut.x * imageWidth + 0.5) / 2.0) > 0.0)
            y = y2 - 0.0625;       // odd so choose Y1
        else
            y = y1 - 0.0625;       // even so choose Y0
    }

    gl_FragColor.r = y + 1.596 * v;
    gl_FragColor.g = y - 0.813 * v - 0.391 * u;
    gl_FragColor.b = y + 2.018 * u;
    gl_FragColor.a = alpha;


    //HDR
//    vec4 intens = smoothstep(0.2,0.8,gl_FragColor) + normalize(vec4(gl_FragColor.xyz, 1.0));
//    if(fract(gl_FragCoord.y * 0.5) > 0.5) intens = gl_FragColor * 0.8;
//    gl_FragColor = intens;


    //HDR
//    //grayscale color
//    vec3 gray = vec3(luma(gl_FragColor.rgb));

//    //threshold red
//    float t = smoothstep(0.8, 0.5, gl_FragColor.r);
//    gl_FragColor.rgb = mix(gl_FragColor.rgb, gray, t);

//    //apply vignette
//    float len = vignette(textureOut, 0.2, 0.65);
//    gl_FragColor.rgb = mix(gl_FragColor.rgb, gl_FragColor.rgb*0.25, len);

//    gl_FragColor.rgb = gl_FragColor.rgb;
//    gl_FragColor.a = 1.0;

}
