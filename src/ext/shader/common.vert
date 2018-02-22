SHADER(

precision highp float;
precision mediump int;

attribute vec2 gpu_Vertex;
attribute vec2 gpu_TexCoord;
attribute mediump vec4 gpu_Color;
uniform mat4 gpu_ModelViewProjectionMatrix;

varying mediump vec4 color;
varying vec2 texCoord;

void main(void)
{
	color = gpu_Color;
	texCoord = vec2(gpu_TexCoord);
	gl_Position = gpu_ModelViewProjectionMatrix * vec4(gpu_Vertex, 0.0, 1.0);
})