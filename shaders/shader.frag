#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_NV_gpu_shader5 : enable

layout (binding = 1) uniform sampler2D samplerColor;

layout (location = 0) in vec2 inUV;
layout (location = 1) in float inLodBias;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;

layout (location = 0) out vec4 outFragColor;

void main()
{
        vec4 color = texture(samplerColor, inUV);


        outFragColor = color;
}