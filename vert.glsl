#version 150

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;

in vec3 in_pos;
in vec3 in_norm;
in vec2 in_uv_coord;
out vec3 frag_pos_eye;
out vec3 frag_norm_eye;
out vec2 frag_uv_coord;

void main() {
	frag_uv_coord = in_uv_coord;
	frag_pos_eye  = vec3(view * model * vec4(in_pos,  1.));
	frag_norm_eye = vec3(view * model * vec4(in_norm, 0.));

	gl_Position = proj * vec4(frag_pos_eye, 1.);
}
