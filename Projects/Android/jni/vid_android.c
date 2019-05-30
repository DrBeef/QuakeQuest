/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "quakedef.h"

#include <signal.h>
//#include <EGL/egl.h>
#include <stdbool.h>

int cl_available = true;

static long oldtime=0;

qboolean vid_supportrefreshrate = false;
extern int vrMode;

void Host_BeginFrame(bool stopTime);
void Host_Frame(int eye, int x, int y);
void Host_EndFrame();

void VID_Shutdown(void)
{
}

static void signal_handler(int sig)
{
	Con_Printf("Received signal %d, exiting...\n", sig);
	Sys_Quit(1);
}

static void InitSig(void)
{
	//Nope, I want a logcat backtrace.
	/*signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);*/
}

void qglBindBufferRange(GLenum target,  GLuint index,  GLuint buffer,  GLintptr offset,  GLsizeiptr size)
{
//Nope.avi
}

void qglUniformBlockBinding(GLuint program,  GLuint uniformBlockIndex,  GLuint uniformBlockBinding)
{
//Nope.avi
}

GLuint qglGetUniformBlockIndex(GLuint program,  const GLchar *uniformBlockName)
{
//Nope.avi
return 0;
}

void glLoadMatrixf(const GLfloat *m)
{
//Nope.avi
}

void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
//Nope.avi
}

void glClientActiveTexture(GLenum target)
{
//Nope.avi
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
//Nope.avi
}

void qglBindFramebuffer(GLenum target, GLuint framebuffer) 
{
glBindFramebuffer(target, framebuffer);
}

void qglBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
glBindRenderbuffer(target, renderbuffer);
}

void qglDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
glDeleteRenderbuffers(n, renderbuffers);
}

void qglDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
glDeleteFramebuffers(n, framebuffers);
}

void qglGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
glGenFramebuffers(n, framebuffers);
}

GLenum qglCheckFramebufferStatus(GLenum target)
{
return glCheckFramebufferStatus(target);
}

void qglGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
glGenRenderbuffers(n, renderbuffers);
}

void qglRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
glRenderbufferStorage(target, internalformat, width, height);
}

void VID_SetMouse (qboolean fullscreengrab, qboolean relative, qboolean hidecursor)
{
}

bool scndswp=0;
void VID_Finish (void)
{
//if (scndswp) eglSwapBuffers(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW));
scndswp=1;
}

int VID_SetGamma(unsigned short *ramps, int rampsize)
{
	return FALSE;
}

int VID_GetGamma(unsigned short *ramps, int rampsize)
{
	return FALSE;
}

void VID_Init(void)
{
	InitSig(); // trap evil signals
}

#define SDL_GL_ExtensionSupported(x) (strstr(gl_extensions, x) || strstr(gl_platformextensions, x))

int is32bit=0;

void GLES_Init(void)
{
	gl_renderer = (const char *)qglGetString(GL_RENDERER);
	gl_vendor = (const char *)qglGetString(GL_VENDOR);
	gl_version = (const char *)qglGetString(GL_VERSION);
	gl_extensions = (const char *)qglGetString(GL_EXTENSIONS);
	
	if (!gl_extensions)
		gl_extensions = "";
	if (!gl_platformextensions)
		gl_platformextensions = "";

	Con_Printf("GL_VENDOR: %s\n", gl_vendor);
	Con_Printf("GL_RENDERER: %s\n", gl_renderer);
	Con_Printf("GL_VERSION: %s\n", gl_version);
	Con_Printf("GL_EXTENSIONS: %s\n", gl_extensions);
	Con_Printf("%s_EXTENSIONS: %s\n", gl_platform, gl_platformextensions);
	
	// LordHavoc: report supported extensions
	Con_Printf("\nQuakeC extensions for server and client: %s\nQuakeC extensions for menu: %s\n", vm_sv_extensions, vm_m_extensions );

	// GLES devices in general do not like GL_BGRA, so use GL_RGBA
	vid.forcetextype = TEXTYPE_RGBA;
	
	vid.support.gl20shaders = true;
	vid.support.amd_texture_texture4 = false;
	vid.support.arb_depth_texture = false;
	vid.support.arb_draw_buffers = false;
	vid.support.arb_multitexture = false;
	vid.support.arb_occlusion_query = false;
	vid.support.arb_shadow = false;
	vid.support.arb_texture_compression = SDL_GL_ExtensionSupported("GL_EXT_texture_compression_s3tc");
	vid.support.arb_texture_cube_map = true;
	vid.support.arb_texture_env_combine = false;
	vid.support.arb_texture_gather = false;
	vid.support.arb_texture_non_power_of_two = strstr(gl_extensions, "GL_OES_texture_npot") != NULL;
	vid.support.arb_vertex_buffer_object = true;
	vid.support.arb_uniform_buffer_object = false;
	vid.support.ati_separate_stencil = false;
	vid.support.ext_blend_minmax = false;
	vid.support.ext_blend_subtract = true;
	vid.support.ext_draw_range_elements = true;
	vid.support.ext_framebuffer_object = false;
	vid.support.ext_packed_depth_stencil = false;
	vid.support.ext_stencil_two_side = false;
	vid.support.ext_texture_3d = SDL_GL_ExtensionSupported("GL_OES_texture_3D");
	vid.support.ext_texture_compression_s3tc = SDL_GL_ExtensionSupported("GL_EXT_texture_compression_s3tc");
	vid.support.ext_texture_edge_clamp = true;
	vid.support.ext_texture_filter_anisotropic = false; // probably don't want to use it...
	vid.support.ext_texture_srgb = false;

	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*)&vid.maxtexturesize_2d);
	if (vid.support.ext_texture_filter_anisotropic)
		qglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint*)&vid.max_anisotropy);
	if (vid.support.arb_texture_cube_map)
		qglGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, (GLint*)&vid.maxtexturesize_cubemap);
	Con_Printf("GL_MAX_CUBE_MAP_TEXTURE_SIZE = %i\n", vid.maxtexturesize_cubemap);
	Con_Printf("GL_MAX_3D_TEXTURE_SIZE = %i\n", vid.maxtexturesize_3d);
	{
#define GL_ALPHA_BITS                           0x0D55
#define GL_RED_BITS                             0x0D52
#define GL_GREEN_BITS                           0x0D53
#define GL_BLUE_BITS                            0x0D54
#define GL_DEPTH_BITS                           0x0D56
#define GL_STENCIL_BITS                         0x0D57
		int fb_r = -1, fb_g = -1, fb_b = -1, fb_a = -1, fb_d = -1, fb_s = -1;
		qglGetIntegerv(GL_RED_BITS    , &fb_r);
		qglGetIntegerv(GL_GREEN_BITS  , &fb_g);
		qglGetIntegerv(GL_BLUE_BITS   , &fb_b);
		qglGetIntegerv(GL_ALPHA_BITS  , &fb_a);
		qglGetIntegerv(GL_DEPTH_BITS  , &fb_d);
		qglGetIntegerv(GL_STENCIL_BITS, &fb_s);
		if ((fb_r+fb_g+fb_b+fb_a)>=24) is32bit=1;
		Con_Printf("Framebuffer depth is R%iG%iB%iA%iD%iS%i\n", fb_r, fb_g, fb_b, fb_a, fb_d, fb_s);
	}

	// verify that cubemap textures are really supported
	if (vid.support.arb_texture_cube_map && vid.maxtexturesize_cubemap < 256)
		vid.support.arb_texture_cube_map = false;
	
	// verify that 3d textures are really supported
	if (vid.support.ext_texture_3d && vid.maxtexturesize_3d < 32)
	{
		vid.support.ext_texture_3d = false;
		Con_Printf("GL_OES_texture_3d reported bogus GL_MAX_3D_TEXTURE_SIZE, disabled\n");
	}

	vid.texunits = 4;
	vid.teximageunits = 8;
	vid.texarrayunits = 5;
	vid.texunits = bound(1, vid.texunits, MAX_TEXTUREUNITS);
	vid.teximageunits = bound(1, vid.teximageunits, MAX_TEXTUREUNITS);
	vid.texarrayunits = bound(1, vid.texarrayunits, MAX_TEXTUREUNITS);
	Con_DPrintf("Using GLES2.0 rendering path - %i texture matrix, %i texture images, %i texcoords%s\n", vid.texunits, vid.teximageunits, vid.texarrayunits, vid.support.ext_framebuffer_object ? ", shadowmapping supported" : "");
	vid.renderpath = RENDERPATH_GLES2;
	vid.useinterleavedarrays = false;
	vid.sRGBcapable2D = false;
	vid.sRGBcapable3D = false;

	// VorteX: set other info (maybe place them in VID_InitMode?)
	extern cvar_t gl_info_vendor;
	extern cvar_t gl_info_renderer;
	extern cvar_t gl_info_version;
	extern cvar_t gl_info_platform;
	extern cvar_t gl_info_driver;
	Cvar_SetQuick(&gl_info_vendor, gl_vendor);
	Cvar_SetQuick(&gl_info_renderer, gl_renderer);
	Cvar_SetQuick(&gl_info_version, gl_version);
	Cvar_SetQuick(&gl_info_platform, gl_platform ? gl_platform : "");
	Cvar_SetQuick(&gl_info_driver, gl_driver);
}

int andrw=640;
int andrh=480;

qboolean VID_InitMode(viddef_mode_t *mode)
{
	mode->width = andrw;
	mode->height = andrh;
	mode->fullscreen = true;
	mode->refreshrate=60.0f;
	vid.softpixels = NULL;
	vid_hidden=false;
	gl_platform = "Android";
	gl_platformextensions = "";
	GLES_Init();
	if (is32bit)
	mode->bitsperpixel=32;else mode->bitsperpixel=16;
	return true;
}

void *GL_GetProcAddress(const char *name)
{
	return NULL;
}

void Sys_SendKeyEvents(void)
{
}

void VID_BuildJoyState(vid_joystate_t *joystate)
{
}

size_t VID_ListModes(vid_mode_t *modes, size_t maxcount)
{
	return 0;
}

typedef struct r_glsl_permutation_s
{
	/// hash lookup data
	struct r_glsl_permutation_s *hashnext;
	unsigned int mode;
	unsigned int permutation;

	/// indicates if we have tried compiling this permutation already
	qboolean compiled;
	/// 0 if compilation failed
	int program;
	// texture units assigned to each detected uniform
	int tex_Texture_First;
	int tex_Texture_Second;
	int tex_Texture_GammaRamps;
	int tex_Texture_Normal;
	int tex_Texture_Color;
	int tex_Texture_Gloss;
	int tex_Texture_Glow;
	int tex_Texture_SecondaryNormal;
	int tex_Texture_SecondaryColor;
	int tex_Texture_SecondaryGloss;
	int tex_Texture_SecondaryGlow;
	int tex_Texture_Pants;
	int tex_Texture_Shirt;
	int tex_Texture_FogHeightTexture;
	int tex_Texture_FogMask;
	int tex_Texture_Lightmap;
	int tex_Texture_Deluxemap;
	int tex_Texture_Attenuation;
	int tex_Texture_Cube;
	int tex_Texture_Refraction;
	int tex_Texture_Reflection;
	int tex_Texture_ShadowMap2D;
	int tex_Texture_CubeProjection;
	int tex_Texture_ScreenNormalMap;
	int tex_Texture_ScreenDiffuse;
	int tex_Texture_ScreenSpecular;
	int tex_Texture_ReflectMask;
	int tex_Texture_ReflectCube;
	int tex_Texture_BounceGrid;
	/// locations of detected uniforms in program object, or -1 if not found
	int loc_Texture_First;
	int loc_Texture_Second;
	int loc_Texture_GammaRamps;
	int loc_Texture_Normal;
	int loc_Texture_Color;
	int loc_Texture_Gloss;
	int loc_Texture_Glow;
	int loc_Texture_SecondaryNormal;
	int loc_Texture_SecondaryColor;
	int loc_Texture_SecondaryGloss;
	int loc_Texture_SecondaryGlow;
	int loc_Texture_Pants;
	int loc_Texture_Shirt;
	int loc_Texture_FogHeightTexture;
	int loc_Texture_FogMask;
	int loc_Texture_Lightmap;
	int loc_Texture_Deluxemap;
	int loc_Texture_Attenuation;
	int loc_Texture_Cube;
	int loc_Texture_Refraction;
	int loc_Texture_Reflection;
	int loc_Texture_ShadowMap2D;
	int loc_Texture_CubeProjection;
	int loc_Texture_ScreenNormalMap;
	int loc_Texture_ScreenDiffuse;
	int loc_Texture_ScreenSpecular;
	int loc_Texture_ReflectMask;
	int loc_Texture_ReflectCube;
	int loc_Texture_BounceGrid;
	int loc_Alpha;
	int loc_BloomBlur_Parameters;
	int loc_ClientTime;
	int loc_Color_Ambient;
	int loc_Color_Diffuse;
	int loc_Color_Specular;
	int loc_Color_Glow;
	int loc_Color_Pants;
	int loc_Color_Shirt;
	int loc_DeferredColor_Ambient;
	int loc_DeferredColor_Diffuse;
	int loc_DeferredColor_Specular;
	int loc_DeferredMod_Diffuse;
	int loc_DeferredMod_Specular;
	int loc_DistortScaleRefractReflect;
	int loc_EyePosition;
	int loc_FogColor;
	int loc_FogHeightFade;
	int loc_FogPlane;
	int loc_FogPlaneViewDist;
	int loc_FogRangeRecip;
	int loc_LightColor;
	int loc_LightDir;
	int loc_LightPosition;
	int loc_OffsetMapping_ScaleSteps;
	int loc_OffsetMapping_LodDistance;
	int loc_OffsetMapping_Bias;
	int loc_PixelSize;
	int loc_ReflectColor;
	int loc_ReflectFactor;
	int loc_ReflectOffset;
	int loc_RefractColor;
	int loc_Saturation;
	int loc_ScreenCenterRefractReflect;
	int loc_ScreenScaleRefractReflect;
	int loc_ScreenToDepth;
	int loc_ShadowMap_Parameters;
	int loc_ShadowMap_TextureScale;
	int loc_SpecularPower;
	int loc_UserVec1;
	int loc_UserVec2;
	int loc_UserVec3;
	int loc_UserVec4;
	int loc_ViewTintColor;
	int loc_ViewToLight;
	int loc_ModelToLight;
	int loc_TexMatrix;
	int loc_BackgroundTexMatrix;
	int loc_ModelViewProjectionMatrix;
	int loc_ModelViewMatrix;
	int loc_PixelToScreenTexCoord;
	int loc_ModelToReflectCube;
	int loc_ShadowMapMatrix;
	int loc_BloomColorSubtract;
	int loc_NormalmapScrollBlend;
	int loc_BounceGridMatrix;
	int loc_BounceGridIntensity;
}
r_glsl_permutation_t;

extern r_glsl_permutation_t *r_glsl_permutation;
extern void android_kostyl();

void QC_BeginFrame(bool stopTime)
{
	scndswp=0;

	Host_BeginFrame(stopTime);
}

void QC_DrawFrame(int eye, int x, int y)
{
	Host_Frame(eye, x, y);
}

void QC_EndFrame()
{
	Host_EndFrame();
}

void QC_KeyEvent(int state,int key,int character)
{
	Key_Event(key, character, state);
}

float analogx=0.0f;
float analogy=0.0f;
int analogenabled=0;
void QC_Analog(int enable,float x,float y)
{
	analogenabled=enable;
	analogx=x;
	analogy=y;
}

void QC_MotionEvent(float delta, float dx, float dy)
{
	static bool canAdjust = true;

	//If not in vr mode, then always use yaw stick control
	if (cl_yawmode.integer == 2)
	{
		in_mouse_x+=(dx*delta);
		in_windowmouse_x += (dx*delta);
		if (in_windowmouse_x<0) in_windowmouse_x=0;
		if (in_windowmouse_x>andrw-1) in_windowmouse_x=andrw-1;
	}
	else if (cl_yawmode.integer == 1) {
		if (fabs(dx) > 0.4 && canAdjust && delta != -1.0f) {
			if (dx > 0.0)
				cl.comfortInc--;
			else
				cl.comfortInc++;

			int max = (360.f / cl_comfort.value);

			if (cl.comfortInc >= max)
				cl.comfortInc = 0;
			if (cl.comfortInc < 0)
				cl.comfortInc = max - 1;

			canAdjust = false;
		}

		if (fabs(dx) < 0.3)
			canAdjust = true;
	}
}

static struct {
	float pitch, previous_pitch, yaw, previous_yaw, roll;
} move_event;


void IN_Move(void)
{
	cl.viewangles[PITCH] = move_event.pitch;

	if (cl_yawmode.integer == 0) {
		cl.viewangles[YAW] = move_event.yaw;
	} else if (cl_yawmode.integer == 1) {
		cl.viewangles[YAW] += move_event.yaw;
	} else
	{
		cl.viewangles[YAW] -= move_event.previous_yaw;
		cl.viewangles[YAW] += move_event.yaw;
	}

	cl.viewangles[ROLL] = move_event.roll ;
}

void QC_MoveEvent(float yaw, float pitch, float roll)
{
	move_event.previous_yaw = move_event.yaw;
	move_event.previous_pitch = move_event.pitch;
	move_event.yaw = yaw * cl_yawmult.value;
	move_event.pitch = pitch * cl_pitchmult.value;
	move_event.roll = roll;
}

void QC_SetResolution(int width, int height)
{
	andrw=width;
	andrh=height;
	VID_Restart_f();
}
