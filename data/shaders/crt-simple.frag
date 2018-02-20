in vec2 texCoord;
out vec4 fragColor;

uniform sampler2D tex;

void main(void)
{
	vec4 rgba = texture2D(tex, texCoord);

	vec4 intensity = fract(gl_FragCoord.y * (0.5 * 4.0 / 3.0)) > 0.5 
		? vec4(0) 
		: smoothstep(0.2, 0.8, rgba) + normalize(rgba);
	
	fragColor = intensity * -0.25 + rgba * 1.1;
}
