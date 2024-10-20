#version 450

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec2 texCoord;
//layout(location = 2) in mat3 tbn;
layout(location = 2) in vec3 normal;

layout(set = 1, binding = 0) uniform sampler2D colorSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;

layout(location = 0) out vec4 outAlbedo;    //留一个通道以后用吧
layout(location = 1) out vec4 outNormal;


void main(){
    outAlbedo = vec4(texture(colorSampler, texCoord).rgb, 1.0);
    vec3 textureNormal = normalize(texture(normalSampler, texCoord)).xyz;

    vec3 tangent = normalize(dFdx(worldPos));
    //vec3 bitangent = normalize(dFdy(worldPos));
    //vec3 normal = normalize(cross(bitangent, tangent));
    vec3 bitangent = normalize(cross(normalize(normal), tangent));
    mat3 TBN = mat3(tangent, bitangent, normal);

    textureNormal = (TBN * textureNormal) * 0.5f + 0.5f;
    outNormal = vec4(textureNormal, 1.0f);
}

