#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec2 lmuv;
layout(location=3) in vec4 color;
layout(location=4) in vec3 normal;
layout(location=5) in vec3 tangent;
layout(location=6) in vec3 bitangent;
layout(set=1,binding=0,std140) uniform Vertex { mat4 mvp; mat4 model; } u;
layout(location=0) out vec2 v_uv;
layout(location=1) out vec2 v_lmuv;
layout(location=2) out vec4 v_color;
layout(location=3) out vec3 v_position;
layout(location=4) out vec3 v_normal;
layout(location=5) out vec3 v_tangent;
layout(location=6) out vec3 v_bitangent;
void main() {
    gl_Position = u.mvp*vec4(position,1);
    v_uv=uv; v_lmuv=lmuv; v_color=color;
    v_position=(u.model*vec4(position,1)).xyz;
    v_normal=mat3(u.model)*normal;
    v_tangent=mat3(u.model)*tangent;
    v_bitangent=mat3(u.model)*bitangent;
}
