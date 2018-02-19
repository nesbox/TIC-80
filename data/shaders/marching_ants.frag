
in vec4 color;
in vec2 texCoord;
out vec4 fragColor;

uniform sampler2D tex;

uniform float resolution_x;
uniform float resolution_y;

uniform float screen_w;
uniform float screen_h;

uniform float time;
uniform float zoom;

void main(void)
{
	float x_offset = 1.0/(screen_w*zoom) + 1.0/(resolution_x*zoom);
	float y_offset = 1.0/(screen_h*zoom) + 1.0/(resolution_y*zoom);
	
	vec4 center = texture2D(tex, texCoord);
	vec4 left = texture2D(tex, vec2(texCoord.s - x_offset, texCoord.t));
	vec4 right = texture2D(tex, vec2(texCoord.s + x_offset, texCoord.t));
	vec4 up = texture2D(tex, vec2(texCoord.s, texCoord.t - y_offset));
	vec4 down = texture2D(tex, vec2(texCoord.s, texCoord.t + y_offset));
	
	bool is_image_edge = (texCoord.s - x_offset < 0 || texCoord.s + x_offset > 1 || texCoord.t - y_offset < 0 || texCoord.t + y_offset > 1);
	
	if(left.a != right.a || left.a != up.a || left.a != down.a || (center.a > 0.5 && is_image_edge))
	{
		if(center.a > 0.5)
		{
			float speed = 0.6;
			float ant_size = 10;
			float black_to_white_ratio = 0.5;
			float ant_position = (gl_FragCoord.x + gl_FragCoord.y);
			float t = step(black_to_white_ratio, mod(time*speed + ant_position/ant_size, 1.0));
			
			fragColor = vec4(t, t, t, 1);
		}
		else
			fragColor = vec4(1, 1, 1, 1);
	}
	else if(center.a < 0.5)
	{
		fragColor = vec4(0, 0, 0, 0.1);
	}
	else
		discard;
}