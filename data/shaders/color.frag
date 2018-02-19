varying vec4 color;
varying vec2 texCoord;

uniform sampler2D tex;
uniform vec4 myColor;

void main(void)
{
    vec4 c = myColor * color;
    gl_FragColor = texture2D(tex, texCoord) * c;
}