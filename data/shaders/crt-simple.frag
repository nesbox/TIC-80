varying vec4 color;
varying vec2 texCoord;

uniform sampler2D tex;
uniform vec4 myColor;

out vec4 fragColor;

void main(void)
{
    vec4 c = myColor * color;

	vec4 rgba = texture2D(tex, texCoord);
    vec4 intensity;

  if(fract(gl_FragCoord.y * (0.5 * 4.0 / 3.0)) > 0.5) {
    intensity = vec4(0);
  } else {
    intensity = smoothstep(0.2, 0.8, rgba) + normalize(rgba);
  }
  
    fragColor = intensity * -0.25 + rgba * 1.1;
}