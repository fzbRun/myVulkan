#version 450

//这里和OpenGl不一样，OpenGl中变量名必须相同且不需要layout，而Vulkan中只需要layout中的location相同即可
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;


void main(){
    outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);
}

