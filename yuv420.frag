//#ifdef GL_ES
//// Set default precision to medium
//precision mediump int;
//precision mediump float;
//#endif

//uniform sampler2D tex_y;
//uniform sampler2D tex_u;
//uniform sampler2D tex_v;

//varying vec2 textureOut;

//const vec3 R_cf = vec3(1.164383,  0.000000,  1.596027);
//const vec3 G_cf = vec3(1.164383, -0.391762, -0.812968);
//const vec3 B_cf = vec3(1.164383,  2.017232,  0.000000);
//const vec3 offset = vec3(-0.0625, -0.5, -0.5);

//void main()
//{
//    vec3 yuv = offset + vec3(texture2D(tex_y, textureOut).r,
//                             texture2D(tex_u, textureOut).r,
//                             texture2D(tex_v, textureOut).r);

//    gl_FragColor = vec4(dot(yuv, R_cf), dot(yuv, G_cf), dot(yuv, B_cf), 1.0);
//}



#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;

varying vec2 textureOut;

const vec3 R_cf = vec3(1.164383,  0.000000,  1.596027);
const vec3 G_cf = vec3(1.164383, -0.391762, -0.812968);
const vec3 B_cf = vec3(1.164383,  2.017232,  0.000000);
const vec3 offset = vec3(-0.0625, -0.5, -0.5);

void main()
{
    vec3 yuv = offset + vec3(texture2D(tex_y, textureOut).r,
                             texture2D(tex_u, textureOut).r,
                             texture2D(tex_v, textureOut).r);

    gl_FragColor = vec4(dot(yuv, R_cf), dot(yuv, G_cf), dot(yuv, B_cf), 1.0);
}
