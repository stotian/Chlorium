// Scalar fields preserve the exact native 24-byte vertex / 96-byte quad ABI.
// vec3 would introduce std430 padding and read the wrong colors/UVs.
struct NativeQuadVertex { float x; float y; float z; uint color; float u; float v; };
struct NativeQuadState { NativeQuadVertex corners[4]; };
layout(std430, binding=4) readonly buffer NativeQuads { NativeQuadState u_quads[]; };
uniform mat4 u_mvp;
// Match the desktop PositionTextureColor shader's perspective interpolation.
// A flat color would drop per-corner gradients; noperspective UVs would change
// sprites under a perspective camera. Desktop Cocos does not use lowp here.
smooth out vec2 t_texCoord;
smooth out vec4 t_color;
void main() {
    NativeQuadVertex vertex = u_quads[gl_InstanceID].corners[gl_VertexID];
    gl_Position = u_mvp * vec4(vertex.x,vertex.y,vertex.z,1);
    t_texCoord = vec2(vertex.u,vertex.v);
    uint rgba = vertex.color;
    // This reproduces the normalized GL_UNSIGNED_BYTE color attribute.
    // Cocos has already applied opacity/premultiplication where appropriate.
    t_color = vec4(rgba & 255u, (rgba >> 8u) & 255u, (rgba >> 16u) & 255u, rgba >> 24u) / 255.0;
}
