// -*- C++ -*-
uniform sampler2D source;

uniform float texture_width;

void main()
{
    vec4 color = vec4(0.0, 0.0, 0.0, 0.0);

    // These textures are 4x3, so we must convert "texture_width" to texture height.
    float pixel_size = 1.0 / (0.75 * texture_width);
    color += texture2D(source, gl_TexCoord[0].st + vec2(0.0, -3.0 * pixel_size)) *  1.0 / 18.0;
    color += texture2D(source, gl_TexCoord[0].st + vec2(0.0, -2.0 * pixel_size)) *  2.0 / 18.0;
    color += texture2D(source, gl_TexCoord[0].st + vec2(0.0, -1.0 * pixel_size)) *  4.0 / 18.0;
    color += texture2D(source, gl_TexCoord[0].st + vec2(0.0,  0.0 * pixel_size)) *  4.0 / 18.0;
    color += texture2D(source, gl_TexCoord[0].st + vec2(0.0,  1.0 * pixel_size)) *  4.0 / 18.0;
    color += texture2D(source, gl_TexCoord[0].st + vec2(0.0,  2.0 * pixel_size)) *  2.0 / 18.0;
    color += texture2D(source, gl_TexCoord[0].st + vec2(0.0,  3.0 * pixel_size)) *  1.0 / 18.0;

    gl_FragColor = color * 0.5;
}
