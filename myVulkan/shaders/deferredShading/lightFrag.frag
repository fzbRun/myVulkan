#version 450

layout(location = 0) in vec2 texCoord;

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputColor;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput inputNormal;
layout (input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput inputDepth;

layout(binding = 0) uniform UniformBufferObject{
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightPos;
    vec3 cameraPos;
} ubo;

layout(location = 0) out vec4 finalColor;

//vulkan的ndc空间坐标与openGL不同，vulkan的ndc的y轴向下为正（逆天）
vec3 getPosFromDepth(float depth, vec2 uv, mat4 transferMat){

    uv = uv * 2.0f - 1.0f;
    vec4 ndc = vec4(uv, depth, 1.0f);
    vec4 pos = transferMat * ndc;
    pos.xyz /= pos.w;

    return pos.xyz;

}

void main() {

    vec4 albedo = sqrt(subpassLoad(inputColor));    //开2.2次根，差不多开平方

    //subPassLoad只能返回与当前像素在纹理上对应的纹素，那么如果我们想做一些blur什么的处理就不行，还是得用采样器
    vec4 normal_tangent = subpassLoad(inputNormal);

    float depth = subpassLoad(inputDepth).x;  //0 - 1，openGL为-1 - 1
    vec2 uv = texCoord;//vec2(texCoord.x, -texCoord.y);
    vec3 worldPos = getPosFromDepth(depth, uv, inverse(ubo.proj * ubo.view));

    //vec4 clip = ubo.proj * ubo.view * ubo.model * vec4(worldPos, 1.0f);
    //vec4 ndc = clip / clip.w;
    //float right = (uv * 2.0f - 1.0f - ndc.xy).y < 0.001f ? 1.0f : 0.0f;
    //right = ndc.z - depth < 0.0001f ? 1.0f : 0.0f;

    vec3 normal = normalize(subpassLoad(inputNormal).xyz * 2.0f - 1.0f);

    //布林冯
    vec3 lightPos = ubo.lightPos;//(ubo.view * vec4(ubo.lightPos, 1.0f)).xyz;
    vec3 cameraPos = ubo.cameraPos;

    normal = inverse(transpose(mat3(ubo.view))) * normal;
    vec3 i = normalize(lightPos - worldPos);
    vec3 o = normalize(cameraPos - worldPos);
    vec3 h = normalize(i + o);
    
    finalColor = vec4(albedo.rgb * pow(max(dot(h, normal), 0.0f), 16), 1.0f) + 0.1f * albedo;//vec4(worldPos, 1.0f);//vec4(dot(o, normal), 0.0f, 0.0f, 1.0f);////vec4(right);//
    //finalColor = viewPos.y > 10.0f ? vec4(1.0f) : vec4(0.0f);
    //finalColor = vec4(o, 1.0f);
}
