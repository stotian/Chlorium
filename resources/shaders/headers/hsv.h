// Keep the object's native floating point HSV values. Packing hue into whole
// degrees and S/V into 8 bits changes even an identity S/V multiplier of 1.
struct ShaderHSV {
    float hue;
    float saturation;
    float value;
    uint flags;
};
#define HSV ShaderHSV
#define HSV_SAT_ADD (1u << 0)
#define HSV_VAL_ADD (1u << 1)

#ifdef GLSL

// Credits to sam hocevar for the following two functions
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec4 applyHSV(HSV hsvValue, vec4 color) {
    float hue = hsvValue.hue / 360.0;
    float sat = hsvValue.saturation;
    float val = hsvValue.value;
    
    vec3 hsv = rgb2hsv(color.rgb);

    hsv.x = mod(hsv.x + hue, 1.0);

    if ((hsvValue.flags & HSV_SAT_ADD) != 0)
        hsv.y += sat;
    else
        hsv.y *= sat;

    if ((hsvValue.flags & HSV_VAL_ADD) != 0)
        hsv.z += val;
    else
        hsv.z *= val;

    hsv.y = clamp(hsv.y, 0.0, 1.0);
    hsv.z = clamp(hsv.z, 0.0, 1.0);
    return vec4(hsv2rgb(hsv), color.a);
}

#endif
