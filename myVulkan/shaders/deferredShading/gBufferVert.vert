#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(binding = 0) uniform UniformBufferObject{
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightPos;
    vec3 cameraPos;
} ubo;

layout(location = 0) out vec3 worldPos;
layout(location = 1) out vec2 texCoord;
//layout(location = 2) out mat3 tbn;
layout(location = 2) out vec3 normal;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    worldPos = (ubo.model * vec4(inPosition, 1.0)).xyz;
    texCoord = inTexCoord;

    //这个模型的纹理uv是镜像的，所以tangent是错误的，我们采用面法线
   mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));
   normal = normalize(normalMatrix * inNormal);
    //tbn = mat3(tangent, bitangent, normal);

}