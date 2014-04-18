#version 150

uniform mat4 view;
uniform mat4 model;
uniform sampler2D the_texture;

in vec3 pos_eye;
in vec3 norm_eye;
in vec2 uv_coord;
out vec4 out_col;

// ambient light
vec3 ambi_color = vec3(.2, .2, .2);

// first (red) light
vec3 light1_pos = vec3(15., 15., 5.);
vec3 diff1_color = vec3(.4, .2, .2);
vec3 spec1_color = vec3(1, .3, .3);
float spec1_exp = 100.;

// second (blue) light
vec3 light2_pos = vec3(-15., 15., -5.);
vec3 diff2_color = vec3(.2, .2, .4);
vec3 spec2_color = vec3(.3, .3, 1);
float spec2_exp = 100.;

// gets the color with respect to a light
vec3 light(vec3 lpos, vec3 dcol, vec3 scol, float exp) {
	vec3 light_eye = vec3(view * vec4(lpos, 1.));
	vec3 dist = lpos - pos_eye;
	vec3 dire = normalize(dist);
	float dd = max(dot(dire, norm_eye), 0.);
	vec3 diffuse = dcol * dd;

	vec3 refl = reflect(-dire, norm_eye);
	vec3 surf2eye = normalize(-pos_eye);
	float ds = max(dot(refl, surf2eye), 0.);
	float fact = pow(ds, exp);
	vec3 specular = scol * fact;

	return diffuse + specular;
}

void main() {
	vec3 color = ambi_color;

	color += light(light1_pos, diff1_color, spec1_color, spec1_exp);
	color += light(light2_pos, diff2_color, spec2_color, spec2_exp);

	out_col = texture2D(the_texture, uv_coord) * vec4(color, 1.0);
}
