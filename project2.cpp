#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <GL/glew.h>

#define GLFW_DLL
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cairo.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>

#ifndef CAIRO_HAS_XLIB_SURFACE
	#error Oh noes!
#endif

extern "C" {
#include "stb_image.c"
}

#define LEVELS_TO_WIN 11
#define NEEDED_TO_WIN 5

#define MOVE_SPEED .03
#define ROT_SPEED  .0003
#define ROT_MULT   1.00001
#define CAM_ROT_SPEED 0.01

#define NEAR ((GLfloat).1)
#define FAR  ((GLfloat)100.)
#define FOV  ((GLfloat)57.)
GLfloat aspect = 640./480.;

static int window_width = 640;
static int window_height = 480;
static int points_got = 0;
static int level = 1;
static int do_reset = 0;

typedef struct {
	GLuint vao;
	GLuint tex;
	GLuint cnt;
} model_t;

static model_t make_model(GLfloat * verts, GLfloat * norms, GLfloat * uv_coords, GLuint vsize, GLuint nsize, GLuint csize, const char * tfn)
{
	model_t m = { 0, 0, vsize/(GLuint)(sizeof(GLfloat)*3) };

	// load texture
	int w, h, n;
	unsigned char * img = stbi_load(tfn, &w, &h, &n, 4);
	assert(img != NULL);

	glGenTextures(1, &m.tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m.tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// create vbo
	GLuint vvbo = 0;
	glGenBuffers(1, &vvbo);
	glBindBuffer(GL_ARRAY_BUFFER, vvbo);
	glBufferData(GL_ARRAY_BUFFER, vsize, verts, GL_STATIC_DRAW);

	GLuint nvbo = 0;
	glGenBuffers(1, &nvbo);
	glBindBuffer(GL_ARRAY_BUFFER, nvbo);
	glBufferData(GL_ARRAY_BUFFER, nsize, norms, GL_STATIC_DRAW);

	GLuint tvbo = 0;
	glGenBuffers(1, &tvbo);
	glBindBuffer(GL_ARRAY_BUFFER, tvbo);
	glBufferData(GL_ARRAY_BUFFER, csize, uv_coords, GL_STATIC_DRAW);

	// create vao
	glGenVertexArrays(1, &m.vao);
	glBindVertexArray(m.vao);

	glBindBuffer(GL_ARRAY_BUFFER, vvbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, nvbo);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, tvbo);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(2);

	return m;
}

static model_t make_cube(const char * tfn)
{
	#include "cube.h"
	return make_model(verts, norms, uv_coords, sizeof(verts), sizeof(norms), sizeof(uv_coords), tfn);
}

static model_t make_plane(void)
{
	#include "plane.h"
	return make_model(verts, norms, uv_coords, sizeof(verts), sizeof(norms), sizeof(uv_coords), "tex/plane.png");
}

static void read_file(const char * fn, GLchar ** buf, GLint * len)
{
	FILE * f = fopen(fn, "rb");
	assert(f != NULL);

	fseek(f, 0, SEEK_END);
	*len = ftell(f);
	fseek(f, 0, SEEK_SET);

	*buf = (char *) malloc(*len);
	fread(*buf, 1, *len, f);

	fclose(f);
}

static GLuint make_shader(const char * fn, GLenum type)
{
	GLchar * buf;
	GLint len;
	read_file(fn, &buf, &len);

	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, (const GLchar **)&buf, &len);
	glCompileShader(s);

	int status;
	glGetShaderiv(s, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) {
		char log[4096];
		glGetShaderInfoLog(s, 4096, NULL, log);
		fprintf(stderr, "error: %s compilation failed\n\n%s", fn, log);
		exit(EXIT_FAILURE);
	}

	free(buf);
	return s;
}

static GLuint make_program(void)
{
	GLuint prog = glCreateProgram();
	glAttachShader(prog, make_shader("vert.glsl", GL_VERTEX_SHADER));
	glAttachShader(prog, make_shader("frag.glsl", GL_FRAGMENT_SHADER));

	glBindAttribLocation(prog, 0, "in_pos");
	glBindAttribLocation(prog, 1, "in_norm");
	glBindAttribLocation(prog, 2, "in_uv_coord");

	glLinkProgram(prog);

	int status;
	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		char log[4096];
		glGetProgramInfoLog(prog, 4096, NULL, log);
		fprintf(stderr, "error: could not link shader program\n\n%s", log);
		exit(EXIT_FAILURE);
	}

	return prog;
}

static void error_callback(int error, const char * desc)
{
	fprintf(stderr, "error (glfw error %d): %s\n", error, desc);
}

// called when window resizes
static void resize_window_callback(GLFWwindow * win, int w, int h)
{
	glViewport(0, 0, w, h);
	aspect = (double)w / (double)h;
}

// vars for keypress states
static int a_down = 0;
static int d_down = 0;
static int w_down = 0;
static int s_down = 0;
static int i_down = 0;
static int j_down = 0;
static int k_down = 0;
static int l_down = 0;
static int q_down = 0;
static int e_down = 0;
static int z_down = 0;
static int x_down = 0;

// updates keypress states
static void keypress_callback(GLFWwindow * win, int key, int sc, int act, int mods)
{
	if (act != GLFW_REPEAT) {
		int state = (act == GLFW_PRESS);
		if (key == 'A') a_down = state;
		if (key == 'D') d_down = state;
		if (key == 'W') w_down = state;
		if (key == 'S') s_down = state;
		if (key == 'I') i_down = state;
		if (key == 'J') j_down = state;
		if (key == 'K') k_down = state;
		if (key == 'L') l_down = state;
		if (key == 'Q') q_down = state;
		if (key == 'E') e_down = state;
		if (key == 'Z') z_down = state;
		if (key == 'X') x_down = state;
		if (key == GLFW_KEY_SPACE && (level < 0 || level >= LEVELS_TO_WIN)) do_reset = 1;
	}
}

static void regmat(GLuint prog, glm::mat4 m, const GLchar * str)
{
	GLuint t = glGetUniformLocation(prog, str);
	glUniformMatrix4fv(t, 1, GL_FALSE, &m[0][0]);
}

static void draw_model(GLuint prog, model_t m, glm::vec3 pos, glm::vec3 rotb, glm::vec3 rota, glm::vec3 scl)
{
	glm::mat4 model;
	model = glm::rotate(model, rotb.x, glm::vec3(1., 0., 0.));
	model = glm::rotate(model, rotb.y, glm::vec3(0., 1., 0.));
	model = glm::rotate(model, rotb.z, glm::vec3(0., 0., 1.));
	model = glm::translate(model, pos);
	model = glm::rotate(model, rota.x, glm::vec3(1., 0., 0.));
	model = glm::rotate(model, rota.y, glm::vec3(0., 1., 0.));
	model = glm::rotate(model, rota.z, glm::vec3(0., 0., 1.));
	model = glm::scale(model, scl);
	regmat(prog, model, "model");

	glBindVertexArray(m.vao);
	glBindTexture(GL_TEXTURE_2D, m.tex);
	glDrawArrays(GL_TRIANGLES, 0, m.cnt);
}

static int collision(glm::vec3 plyr, glm::vec3 pnt)
{
#define BOUND 1.4
	return fabs(plyr.x - pnt.x) < BOUND && fabs(plyr.z - pnt.z) < BOUND;
#undef BOUND
}

static glm::vec3 rand_pos(glm::vec3 plyr)
{
	GLfloat x;
	GLfloat z;
	glm::vec3 r;

	do {
		x = (GLfloat)(random() % 20 - 10);
		z = (GLfloat)(random() % 20 - 10);
		r = glm::vec3(x, 0., z);
	} while (collision(plyr, r));

	return r;
}

static void out_text(GLFWwindow * window, const char * txt)
{
	// this function gets a x11 window and creates a cairo instance from it to
	// draw to text to it

	static int first = 1;
	static cairo_t * cr;

	if (first) {
		Display * xlib_dsp = glfwGetX11Display();
		Window xlib_win = glfwGetX11Window(window);

		XWindowAttributes attr;
		assert(XGetWindowAttributes(xlib_dsp, xlib_win, &attr));
		Visual * xlib_vis = attr.visual;

		cairo_surface_t * surf = cairo_xlib_surface_create(xlib_dsp, xlib_win, xlib_vis, window_width, window_height);
		cr = cairo_create(surf);
		first = 0;
	}

	cairo_set_source_rgb(cr, 0., 1., 0.);
	cairo_select_font_face(cr, "Helvetica", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 14);
	cairo_move_to(cr, 10, 20);
	cairo_show_text(cr, txt);
}

int main()
{
	glfwSetErrorCallback(error_callback);

	// initialize glfw
	if (!glfwInit()) {
		fprintf(stderr, "error: could not start glfw.\n");
		return EXIT_FAILURE;
	}
	atexit(glfwTerminate);

	// glfw hints
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4);

	// create glfw window
	GLFWwindow * window = glfwCreateWindow(window_width, window_height, "Project 2", NULL, NULL);
	assert(window != NULL);
	glfwMakeContextCurrent(window);

	// setup callbacks
	glfwSetWindowSizeCallback(window, resize_window_callback);
	glfwSetKeyCallback(window, keypress_callback);

	// initialize glew
	glewExperimental = GL_TRUE;
	assert(glewInit() == GLEW_OK);


	glfwSwapInterval(1);

	// enable depth buffer
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	srandom(time(NULL));

	// make stuff
	model_t cube = make_cube("tex/plyr.png");
	model_t plane = make_plane();
	model_t point = make_cube("tex/point.png");
	GLuint prog = make_program();

	glm::vec3 plyr_pos(0., 0.001, 0.);
	glm::vec3 plyr_rot(0., 0., 0.);
	glm::vec3 pln_rot (0., 0., 0.);
	glm::vec3 pnt_pos = rand_pos(plyr_pos);
	glm::vec3 pnt_rot (M_PI/4., 0., 0.);

	GLfloat xrinc = 0.;
	GLfloat zrinc = 0.;
	GLfloat cam_angle = 0.;

	// level vars
	GLfloat rot_speed = ROT_SPEED;
	GLfloat rot_mult = ROT_MULT;
	GLfloat plyr_speed_div = 3.;

	// main loop
	double ptime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		// this could probably be done cleaner
		if (do_reset) {
			plyr_pos.x = 0.;
			plyr_pos.z = 0.;
			plyr_rot = glm::vec3(0.);
			pln_rot  = glm::vec3(0.);
			pnt_pos  = rand_pos(plyr_pos);
			pnt_rot  = glm::vec3(M_PI/4., 0., 0.);
			xrinc = zrinc = 0.;
			do_reset = 0;
			points_got = 0;
			cam_angle = M_PI/4.;
			level = 1;
			rot_speed = ROT_SPEED;
			rot_mult = ROT_MULT;
			plyr_speed_div = 3.;
		}

		double time = glfwGetTime();
		if (time - ptime > .01) {
			ptime = time;

			if (level >= 0 && level < LEVELS_TO_WIN) {
				// check for collision with points
				if (collision(plyr_pos, pnt_pos)) {
					pnt_pos = rand_pos(plyr_pos);
					points_got++;
					if (points_got >= NEEDED_TO_WIN) {
						points_got = 0;
						switch ((++level) % 2) {
						case 0:
							rot_speed += ROT_SPEED / 2.;
							break;
						case 1:
							plyr_speed_div /= 1.5;
						case 2:
							rot_mult *= ROT_MULT;
						}
					}
				}

				if (plyr_pos.x < -10.5 || plyr_pos.z < -10.5 || plyr_pos.x > 10.5 || plyr_pos.z > 10.5) {
					level = -level;
				}

				// update positions
				if (a_down) xrinc += rot_speed;
				if (d_down) xrinc -= rot_speed;
				if (w_down) zrinc += rot_speed;
				if (s_down) zrinc -= rot_speed;

				xrinc *= rot_mult;
				zrinc *= rot_mult;

				pln_rot.x += xrinc;
				pln_rot.z += zrinc;

				plyr_rot.x = pln_rot.x;
				plyr_rot.z = pln_rot.z;
			}

			pnt_rot.y += .005 * level;
			pnt_rot.x += .005 * level;
			plyr_pos.x -= (fmod(pln_rot.z, M_PI*2.) / plyr_speed_div);
			plyr_pos.z += (fmod(pln_rot.x, M_PI*2.) / plyr_speed_div);

			// control camera
			if (z_down) cam_angle += CAM_ROT_SPEED;
			if (x_down) cam_angle -= CAM_ROT_SPEED;

			cam_angle *= .99;

			// update matricies
			glm::mat4 view = glm::lookAt(
				glm::vec3(cos(cam_angle+M_PI/4.)*40., 20., sin(cam_angle+M_PI/4.)*40.),
				glm::vec3(0., 0., 0.),
				glm::vec3(0., 1., 0.)
			);

			glm::mat4 proj = glm::perspective(FOV, aspect, NEAR, FAR);

			regmat(prog, view, "view");
			regmat(prog, proj, "proj");

			// redraw
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glUseProgram(prog);

			draw_model(prog, cube,  plyr_pos, plyr_rot, glm::vec3(0.), glm::vec3(1.));
			draw_model(prog, plane, glm::vec3(0, -1., 0), pln_rot, glm::vec3(0.), glm::vec3(1.));
			draw_model(prog, point, pnt_pos, pln_rot, pnt_rot, glm::vec3(.5));

			char buf[64];
			if (level < 0) {
				sprintf(buf, "You lose with %d points on level %d.\n", points_got, -level);
				out_text(window, buf);
			} else if (level == LEVELS_TO_WIN) {
				out_text(window, "You win!");
			} else {
				sprintf(buf, "Points: %d\nLevel: %d", points_got, level);
				out_text(window, buf);
			}

			glfwPollEvents();
			glfwSwapBuffers(window);
		}
	}

	return 0;
}
