#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(set=1,binding=0,std140) uniform Vertex { mat4 mvp; } u;
layout(location=0) out vec2 v_uv;
void main() { gl_Position=u.mvp*vec4(position,1); v_uv=uv; }
