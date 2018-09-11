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


void main() {
    if(textureOut.x > 1.0 - tex_offset){
        gl_FragColor.a = 0;
        gl_FragColor.r = 0;
        gl_FragColor.g = 0;
        gl_FragColor.b = 0;
        return;
    }
    vec4 uyvy = texture2D( tex_y, textureOut);

    float y;
    float u;
    float v;
    float y1;
    float y2;
    float y3;
    float y4;
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
        u = texture2D( tex_u, textureOut).r - 0.5;
        v = texture2D( tex_v, textureOut).r - 0.5;
        y = uyvy.r - 0.0625;
    }else if(tex_format == 11){ //BGR8
        gl_FragColor.rgb = uyvy.rgb;
    }else if(tex_format == 12){ //RGBA
        gl_FragColor.rgba = uyvy.rgba; //RGBA
    }else if(tex_format == 13){//ARGB
        gl_FragColor.argb = uyvy.rgba; //ARGB
    }else if(tex_format == 14){//ABGR
        gl_FragColor.abgr = uyvy.rgba; //ABGR
    }else if(tex_format == 15){//BGRA
        gl_FragColor.bgra = uyvy.rgba; //BGRA
    }

    if(tex_format == 5 || tex_format == 6){ //UYUV | YUYV
        if (fract(floor(textureOut.x * imageWidth + 0.5) / 2.0) > 0.0)
            y = y2 - 0.0625;       // odd so choose Y1
        else
            y = y1 - 0.0625;       // even so choose Y0
    }

    if(tex_format == 5 || tex_format == 6){
        gl_FragColor.r = y + 1.596 * v;
        gl_FragColor.g = y - 0.813 * v - 0.391 * u;
        gl_FragColor.b = y + 2.018 * u;
        gl_FragColor.a = alpha;
    }else if(tex_format == 12 || tex_format == 13 || tex_format == 14 || tex_format == 15){
        //        RGBA = 12 ,
        //        ARGB = 13 ,
        //        ABGR = 14 ,
        //        BGRA = 15 ,
        gl_FragColor *= alpha;
    }else if(tex_format == 18){//BGR4
        gl_FragColor.rgb = uyvy.rgb;
        gl_FragColor.a = alpha;
    }else if(tex_format == 24){//RGB444
        gl_FragColor.rgb = uyvy.gba;
        gl_FragColor.a = alpha;
    }else if(tex_format == 25){//BGR444
        gl_FragColor.rgb = uyvy.abg;
        gl_FragColor.a = alpha;
    }else{
        gl_FragColor.r = y + 1.596 * v;
        gl_FragColor.g = y - 0.813 * v - 0.391 * u;
        gl_FragColor.b = y + 2.018 * u;
        gl_FragColor.a = alpha;
    }


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
