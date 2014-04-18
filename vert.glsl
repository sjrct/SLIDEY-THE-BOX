#version 150

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;

in vec3 in_pos;
in vec3 in_norm;
in vec2 in_uv_coord;
out vec3 pos_eye;
out vec3 norm_eye;
out vec2 uv_coord;

void main() {
	uv_coord = in_uv_coord;
	pos_eye  = vec3(view * model * vec4(in_pos,  1.));
	norm_eye = vec3(view * model * vec4(in_norm, 0.));

	gl_Position = proj * vec4(pos_eye, 1.);
}
