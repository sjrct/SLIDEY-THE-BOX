#version 150

uniform mat4 view;
uniform mat4 model;
uniform sampler2D the_texture;

in vec3 frag_pos_eye;
in vec3 frag_norm_eye;
in vec2 frag_uv_coord;
out vec4 out_col;

// ambient light
vec3 Ia = vec3(.1, .1, .1);

// first (red) light
vec3 light1_pos = vec3(5., 3., -5.);
vec3 diff1_color = vec3(.4, .2, .2);
vec3 spec1_color = vec3(1, .3, .3);
float spec1_exp = 100.;

// second light
vec3 light2_pos = vec3(-5., 10., -5.);
vec3 diff2_color = vec3(.2, .2, .55);
vec3 spec2_color = vec3(.3, .3, 1);
float spec2_exp = 100.;

// third light
vec3 light3_pos = vec3(-20., 5., -20.);
vec3 diff3_color = vec3(.5, .5, .5);
vec3 spec3_color = vec3(1., 1., 1);
float spec3_exp = 100.;

// gets the color with respect to a light
vec3 light(vec3 lpos, vec3 Id, vec3 Is, float alpha)
{
	vec3 light_eye = vec3(view * vec4(lpos, 1.));
	vec3 dist = light_eye - frag_pos_eye;
	vec3 dire = normalize(dist);
	float dd = max(dot(dire, frag_norm_eye), 0.);
	vec3 Fd = Id  * dd;

	vec3 refl = reflect(-dire, frag_norm_eye);
	vec3 surf2eye = normalize(-frag_pos_eye);
	float ds = max(dot(refl, surf2eye), 0.);
	float fact = pow(ds, alpha);
	vec3 Fs = Is * fact;

	return Fd + Fs;
}

void main() {
	vec3 Ip = Ia;

	Ip += light(light1_pos, diff1_color, spec1_color, spec1_exp);
	Ip += light(light2_pos, diff2_color, spec2_color, spec2_exp);
	Ip += light(light3_pos, diff3_color, spec3_color, spec3_exp);

	out_col = texture2D(the_texture, frag_uv_coord) * vec4(Ip, 1.0);
//	out_col = fake_bloom() * vec4(Ip, 1.0);
}
