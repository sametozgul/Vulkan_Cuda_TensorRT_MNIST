#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_NV_gpu_shader5 : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

layout (binding = 0) uniform UBO
{
        mat4 projection;
        mat4 model;
        mat4 view;
        } ubo;

layout (location = 0) out vec2 outUV;
layout (location = 1) out float outLodBias;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outLightVec;

out gl_PerVertex
{
        vec4 gl_Position;
};

void main()
{
        outUV = inUV;


        vec3 worldPos = vec3(ubo.model * vec4(inPos, 1.0));

        gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPos.xyz, 1.0);
}