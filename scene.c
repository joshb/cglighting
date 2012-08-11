/*
 * Copyright (C) 2004 Josh A. Beam
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#ifdef _WIN32
	#include <windows.h>
	#include <wingdi.h>
	#define GLUT_DISABLE_ATEXIT_HACK
#else
	#include <sys/time.h>
	#include <GL/glx.h>
#endif /* _WIN32 */
#include <GL/glut.h>

#ifndef GL_FRAGMENT_PROGRAM_ARB
#define GL_FRAGMENT_PROGRAM_ARB		0x8804
#endif
#ifndef GL_PROGRAM_FORMAT_ASCII_ARB
#define GL_PROGRAM_FORMAT_ASCII_ARB	0x8875
#endif

extern unsigned char *read_pcx(const char *, unsigned int *, unsigned int *);

static void (*my_glGenProgramsARB)(GLuint, GLuint *) = NULL;
static void (*my_glBindProgramARB)(GLuint, GLuint) = NULL;
static void (*my_glProgramStringARB)(GLuint, GLuint, GLint, const GLbyte *) = NULL;

static void (*my_glActiveTextureARB)(GLenum) = NULL;
static void (*my_glMultiTexCoord3fARB)(GLenum, GLfloat, GLfloat, GLfloat) = NULL;

static void *
get_proc_address(const char *name)
{
#ifdef _WIN32
	return (void *)wglGetProcAddress(name);
#else
	return (void *)glXGetProcAddress((const GLubyte *)name);
#endif /* _WIN32 */
}

static void
set_function_pointers(void)
{
	my_glGenProgramsARB = (void (*)(GLuint, GLuint *))get_proc_address("glGenProgramsARB");
	if(!my_glGenProgramsARB) {
		fprintf(stderr, "set_function_pointers(): glGenProgramsARB failed\n");
		exit(1);
	}
	my_glBindProgramARB = (void (*)(GLuint, GLuint))get_proc_address("glBindProgramARB");
	if(!my_glBindProgramARB) {
		fprintf(stderr, "set_function_pointers(): glBindProgramARB failed\n");
		exit(1);
	}
	my_glProgramStringARB = (void (*)(GLuint, GLuint, GLint, const GLbyte *))get_proc_address("glProgramStringARB");
	if(!my_glProgramStringARB) {
		fprintf(stderr, "set_function_pointers(): glProgramStringARB failed\n");
		exit(1);
	}
	my_glActiveTextureARB = (void (*)(GLuint))get_proc_address("glActiveTexture");
	if(!my_glActiveTextureARB) {
		fprintf(stderr, "set_function_pointers(): glActiveTextureARB failed\n");
		exit(1);
	}
	my_glMultiTexCoord3fARB = (void (*)(GLuint, GLfloat, GLfloat, GLfloat))get_proc_address("glMultiTexCoord3fARB");
	if(!my_glMultiTexCoord3fARB) {
		fprintf(stderr, "set_function_pointers(): glMultiTexCoord3fARB failed\n");
		exit(1);
	}
}

/*
 * this function returns a string containing
 * the contents of the specified shader file
 */
static char *
load_program_string(const char *filename)
{
	static char program_string[16384];
	FILE *fp;
	unsigned int len;

	fp = fopen(filename, "r");
	if(!fp)
		return NULL;

	len = fread(program_string, 1, 16384, fp);
	program_string[len] = '\0';
	fclose(fp);

	return program_string;
}

/*
 * this function loads the shader from the specified
 * file and returns a shader number that can be passed
 * to ARB_fragment_program functions
 */
static unsigned int
load_shader(GLuint type, const char *filename)
{
	unsigned int shader_num;
	char *program_string;

	program_string = load_program_string(filename);

	glEnable(type);
	my_glGenProgramsARB(1, &shader_num);
	my_glBindProgramARB(type, shader_num);
	my_glProgramStringARB(type, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(program_string), (const GLbyte *)program_string);
	glDisable(type);

	return shader_num;
}

static unsigned int
load_texture(const char *filename)
{
	unsigned int tex_num;
	unsigned char *data;
	unsigned int width, height;

	data = read_pcx(filename, &width, &height);
	if(!data)
		return 0;

	glGenTextures(1, &tex_num);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex_num);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);

	free(data);

	return tex_num;
}

static unsigned int shader_num;
static unsigned int tex_num;

static float camrot[3] = { 0.0f, 0.0f, 0.0f };
static float lightpos[3] = { 0.0f, 0.0f, 0.0f };

void
scene_init(void)
{
	set_function_pointers();
	shader_num = load_shader(GL_FRAGMENT_PROGRAM_ARB, "shader.pso");
	tex_num = load_texture("texture.pcx");
}

void
scene_render(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -5.0f);
	glRotatef(camrot[0], 1.0f, 0.0f, 0.0f);
	glRotatef(camrot[1], 0.0f, 1.0f, 0.0f);
	glRotatef(camrot[2], 0.0f, 0.0f, 1.0f);

/*
 * the VERTEX macro basically puts (x,y,z) - lightpos into the second
 * texture unit's texture coordinates and calls glVertex3f((x), (y), (z));
 */
#define VERTEX(x,y,z) \
	my_glMultiTexCoord3fARB(GL_TEXTURE1_ARB, (x) - lightpos[0], (y) - lightpos[1], (z) - lightpos[2]); \
	glVertex3f((x), (y), (z));

	glColor3f(1.0f, 1.0f, 1.0f);
	glBindTexture(GL_TEXTURE_2D, tex_num);

	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	my_glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, shader_num);

	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f);
		VERTEX(-1.0f, 1.0f, 0.0f);
		glTexCoord2f(0.0f, 1.0f);
		VERTEX(-1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		VERTEX(1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		VERTEX(1.0f, 1.0f, 0.0f);

		glTexCoord2f(0.0f, 0.0f);
		VERTEX(-1.0f, 1.0f, 2.0f);
		glTexCoord2f(0.0f, 1.0f);
		VERTEX(-1.0f, 1.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		VERTEX(-1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		VERTEX(-1.0f, -1.0f, 2.0f);

		glTexCoord2f(0.0f, 0.0f);
		VERTEX(1.0f, -1.0f, 2.0f);
		glTexCoord2f(0.0f, 1.0f);
		VERTEX(1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		VERTEX(1.0f, 1.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		VERTEX(1.0f, 1.0f, 2.0f);

		glTexCoord2f(0.0f, 0.0f);
		VERTEX(-1.0f, 1.0f, 2.0f);
		glTexCoord2f(0.0f, 1.0f);
		VERTEX(-1.0f, 1.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		VERTEX(1.0f, 1.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		VERTEX(1.0f, 1.0f, 2.0f);

		glTexCoord2f(0.0f, 0.0f);
		VERTEX(-1.0f, -1.0f, 2.0f);
		glTexCoord2f(0.0f, 1.0f);
		VERTEX(-1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		VERTEX(1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		VERTEX(1.0f, -1.0f, 2.0f);
	glEnd();

	glDisable(GL_FRAGMENT_PROGRAM_ARB);

	glutSwapBuffers();
}

static unsigned int
get_ticks(void)
{       
#ifdef _WIN32
	return GetTickCount();
#else
	struct timeval t;

	gettimeofday(&t, NULL);

	return (t.tv_sec * 1000) + (t.tv_usec / 1000);
#endif /* _WIN32 */
}

void
scene_cycle(void)
{
	static float lightrot = 0.0f;
	static unsigned int prev_ticks = 0;
	unsigned int ticks;
	float time;

	if(!prev_ticks)
		prev_ticks = get_ticks();

	ticks = get_ticks();
	time = (float)(ticks - prev_ticks) / 100.0f;
	prev_ticks = ticks;

	camrot[1] += 1.0f * time;
	lightrot += (M_PI / 180.0f) * time;

	lightpos[0] = cosf(lightrot) * 1.0f;
	lightpos[1] = sinf(lightrot) * 1.0f;

	scene_render();
}
