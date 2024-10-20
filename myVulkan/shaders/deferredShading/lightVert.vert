#version 450

layout(location = 0) out vec2 texCoord;

void main() {
/*
	gl_Position = vec4(
		gl_VertexIndex <= 1 ? -1.0f : 3.0f,
		gl_VertexIndex == 0 ? 3.0f : -1.0f,
		0.0f, 1.0f
	);
	texCoord = vec2(
		gl_VertexIndex <= 1 ? 0.0f : 2.0f,
		gl_VertexIndex == 0 ? 2.0f : 0.0f
	);
*/
	texCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(texCoord * 2.0f + -1.0f, 0.0f, 1.0f);

}