#ifndef COLOUR_GLSL
#define COLOUR_GLSL

float lum(vec3 col) { return dot(col, vec3(0.2126, 0.7152, 0.0722)); }

#endif // COLOUR_GLSL
