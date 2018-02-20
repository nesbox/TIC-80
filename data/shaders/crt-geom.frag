#define distortion 0.08

in vec2 texCoord;
out vec4 fragColor;

uniform sampler2D tex;

vec2 radialDistortion(vec2 coord) {
  vec2 cc = coord - vec2(0.5);
  float dist = dot(cc, cc) * distortion;
  return coord + cc * (1.0 - dist) * dist;
}

void main(void) {
	vec4 rgba = texture2D(tex, radialDistortion(texCoord));

	vec4 intensity = fract(gl_FragCoord.y * (0.5 * 4.0 / 3.0)) > 0.5 
		? vec4(0) 
		: smoothstep(0.2, 0.8, rgba) + normalize(rgba);
	
	fragColor = intensity * -0.25 + rgba * 1.1;

}