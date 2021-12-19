/************************************************************************************

Filename	:	QuakeQuest_SurfaceView.c based on VrCubeWorld_SurfaceView.c
Content		:	This sample uses a plain Android SurfaceView and handles all
				Activity and Surface life cycle events in native code. This sample
				does not use the application framework and also does not use LibOVR.
				This sample only uses the VrApi.
Created		:	March, 2015
Authors		:	J.M.P. van Waveren / Simon Brown

Copyright	:	Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>					// for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h>	// for native window JNI
#include <android/input.h>

#include "argtable3.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include "../darkplaces/qtypes.h"
#include "../darkplaces/quakedef.h"
#include "../darkplaces/menu.h"

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"
#include "VrApi_Types.h"

#include "VrCompositor.h"


#include "VrCommon.h"

#if !defined( EGL_OPENGL_ES3_BIT_KHR )
#define EGL_OPENGL_ES3_BIT_KHR		0x0040
#endif

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER			0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR		0x1004
#endif

#if !defined( GL_EXT_multisampled_render_to_texture )
typedef void (GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);
#endif

#if !defined( GL_OVR_multiview )
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR       = 0x9630;
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR = 0x9632;
static const int GL_MAX_VIEWS_OVR                                      = 0x9631;
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews);
#endif

#if !defined( GL_OVR_multiview_multisampled_render_to_texture )
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLsizei samples, GLint baseViewIndex, GLsizei numViews);
#endif

// Must use EGLSyncKHR because the VrApi still supports OpenGL ES 2.0
#define EGL_SYNC

#if defined EGL_SYNC
// EGL_KHR_reusable_sync
PFNEGLCREATESYNCKHRPROC			eglCreateSyncKHR;
PFNEGLDESTROYSYNCKHRPROC		eglDestroySyncKHR;
PFNEGLCLIENTWAITSYNCKHRPROC		eglClientWaitSyncKHR;
PFNEGLSIGNALSYNCKHRPROC			eglSignalSyncKHR;
PFNEGLGETSYNCATTRIBKHRPROC		eglGetSyncAttribKHR;
#endif

//Let's go to the maximum!
int CPU_LEVEL			= 4;
int GPU_LEVEL			= 4;
int REFRESH			    = -1;
int NUM_MULTI_SAMPLES	= 1;
float SS_MULTIPLIER    = 1.2f;

float selectedFramerate=60.0; //The lowest default framerate

extern float worldPosition[3];
float hmdPosition[3];
float playerHeight;
float positionDeltaThisFrame[3];

extern cvar_t vr_worldscale;
extern cvar_t r_lasersight;
extern cvar_t cl_movementspeed;
extern cvar_t cl_walkdirection;
extern cvar_t cl_controllerdeadzone;
extern cvar_t cl_righthanded;
extern cvar_t vr_weaponpitchadjust;
extern cvar_t slowmo;
extern cvar_t bullettime;
extern cvar_t cl_trackingmode;

ovrDeviceID controllerIDs[2];


jclass clazz;

float radians(float deg) {
	return (deg * M_PI) / 180.0;
}

float degrees(float rad) {
	return (rad * 180.0) / M_PI;
}

/* global arg_xxx structs */
struct arg_dbl *ss;
struct arg_int *cpu;
struct arg_int *gpu;
struct arg_int *refresh;
struct arg_dbl *msaa;
struct arg_end *end;

char **argv;
int argc=0;


/*
================================================================================

System Clock Time in millis

================================================================================
*/

double GetTimeInMilliSeconds()
{
	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	return ( now.tv_sec * 1e9 + now.tv_nsec ) * (double)(1e-6);
}

/*
================================================================================

QuakeQuest Stuff

================================================================================
*/

//All the functionality we link to in the DarkPlaces Quake implementation
extern void QC_BeginFrame();
extern void QC_DrawFrame(int eye, int x, int y);
extern void QC_EndFrame();
extern void QC_GetAudio();
extern void QC_KeyEvent(int state,int key,int character);
extern void QC_MoveEvent(float yaw, float pitch, float roll);
extern void QC_SetCallbacks(void *init_audio, void *write_audio);
extern void QC_SetResolution(int width, int height);
extern void QC_Analog(int enable,float x,float y);
extern void QC_MotionEvent(float delta, float dx, float dy);
extern int main (int argc, char **argv);
extern	int			key_consoleactive;

static bool quake_initialised = false;

static JavaVM *jVM;
static jobject qquestCallbackObj=0;


//Timing stuff for joypad control
static long oldtime=0;
long delta=0;

int curtime;
int Sys_Milliseconds (void)
{
    struct timeval tp;
    struct timezone tzp;
    static int		secbase;

    gettimeofday(&tp, &tzp);

    if (!secbase)
    {
        secbase = tp.tv_sec;
        return tp.tv_usec/1000;
    }

    curtime = (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;

    return curtime;
}
int runStatus = -1;
void QC_exit(int exitCode)
{
	runStatus = exitCode;
}

vec3_t hmdorientation;
extern float gunangles[3];
float weaponOffset[3];
float weaponVelocity[3];
qboolean weapon_stabilised;

float vrFOV;

int hmdType;

int bigScreen = 1;
extern client_static_t	cls;

qboolean useScreenLayer()
{
    //TODO
    return (bigScreen != 0 || cls.demoplayback || key_consoleactive);
}

void BigScreenMode(int mode)
{
	if (bigScreen != 2)
	{
		bigScreen = mode;
	}
}

extern int stereoMode;

static void UnEscapeQuotes( char *arg )
{
	char *last = NULL;
	while( *arg ) {
		if( *arg == '"' && *last == '\\' ) {
			char *c_curr = arg;
			char *c_last = last;
			while( *c_curr ) {
				*c_last = *c_curr;
				c_last = c_curr;
				c_curr++;
			}
			*c_last = '\0';
		}
		last = arg;
		arg++;
	}
}

static int ParseCommandLine(char *cmdline, char **argv)
{
	char *bufp;
	char *lastp = NULL;
	int argc, last_argc;
	argc = last_argc = 0;
	for ( bufp = cmdline; *bufp; ) {
		while ( isspace(*bufp) ) {
			++bufp;
		}
		if ( *bufp == '"' ) {
			++bufp;
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ( *bufp != '"' || *lastp == '\\' ) ) {
				lastp = bufp;
				++bufp;
			}
		} else {
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ! isspace(*bufp) ) {
				++bufp;
			}
		}
		if ( *bufp ) {
			if ( argv ) {
				*bufp = '\0';
			}
			++bufp;
		}
		if( argv && last_argc != argc ) {
			UnEscapeQuotes( argv[last_argc] );
		}
		last_argc = argc;
	}
	if ( argv ) {
		argv[argc] = NULL;
	}
	return(argc);
}

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

typedef struct
{
	bool multi_view;						// GL_OVR_multiview, GL_OVR_multiview2
	bool EXT_texture_border_clamp;			// GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
} OpenGLExtensions_t;

OpenGLExtensions_t glExtensions;

static void EglInitExtensions()
{
#if defined EGL_SYNC
	eglCreateSyncKHR		= (PFNEGLCREATESYNCKHRPROC)			eglGetProcAddress( "eglCreateSyncKHR" );
	eglDestroySyncKHR		= (PFNEGLDESTROYSYNCKHRPROC)		eglGetProcAddress( "eglDestroySyncKHR" );
	eglClientWaitSyncKHR	= (PFNEGLCLIENTWAITSYNCKHRPROC)		eglGetProcAddress( "eglClientWaitSyncKHR" );
	eglSignalSyncKHR		= (PFNEGLSIGNALSYNCKHRPROC)			eglGetProcAddress( "eglSignalSyncKHR" );
	eglGetSyncAttribKHR		= (PFNEGLGETSYNCATTRIBKHRPROC)		eglGetProcAddress( "eglGetSyncAttribKHR" );
#endif

	const char * allExtensions = (const char *)glGetString( GL_EXTENSIONS );
	if ( allExtensions != NULL )
	{
		glExtensions.multi_view = strstr( allExtensions, "GL_OVR_multiview2" ) &&
								  strstr( allExtensions, "GL_OVR_multiview_multisampled_render_to_texture" );

		glExtensions.EXT_texture_border_clamp = false;//strstr( allExtensions, "GL_EXT_texture_border_clamp" ) ||
												//strstr( allExtensions, "GL_OES_texture_border_clamp" );
	}
}

static const char * EglErrorString( const EGLint error )
{
	switch ( error )
	{
		case EGL_SUCCESS:				return "EGL_SUCCESS";
		case EGL_NOT_INITIALIZED:		return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS:			return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC:				return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE:			return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONTEXT:			return "EGL_BAD_CONTEXT";
		case EGL_BAD_CONFIG:			return "EGL_BAD_CONFIG";
		case EGL_BAD_CURRENT_SURFACE:	return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY:			return "EGL_BAD_DISPLAY";
		case EGL_BAD_SURFACE:			return "EGL_BAD_SURFACE";
		case EGL_BAD_MATCH:				return "EGL_BAD_MATCH";
		case EGL_BAD_PARAMETER:			return "EGL_BAD_PARAMETER";
		case EGL_BAD_NATIVE_PIXMAP:		return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW:		return "EGL_BAD_NATIVE_WINDOW";
		case EGL_CONTEXT_LOST:			return "EGL_CONTEXT_LOST";
		default:						return "unknown";
	}
}

static const char * GlFrameBufferStatusString( GLenum status )
{
	switch ( status )
	{
		case GL_FRAMEBUFFER_UNDEFINED:						return "GL_FRAMEBUFFER_UNDEFINED";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:			return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:	return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_UNSUPPORTED:					return "GL_FRAMEBUFFER_UNSUPPORTED";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:			return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
		default:											return "unknown";
	}
}


/*
================================================================================

ovrEgl

================================================================================
*/

typedef struct
{
	EGLint		MajorVersion;
	EGLint		MinorVersion;
	EGLDisplay	Display;
	EGLConfig	Config;
	EGLSurface	TinySurface;
	EGLSurface	MainSurface;
	EGLContext	Context;
} ovrEgl;

static void ovrEgl_Clear( ovrEgl * egl )
{
	egl->MajorVersion = 0;
	egl->MinorVersion = 0;
	egl->Display = 0;
	egl->Config = 0;
	egl->TinySurface = EGL_NO_SURFACE;
	egl->MainSurface = EGL_NO_SURFACE;
	egl->Context = EGL_NO_CONTEXT;
}

static void ovrEgl_CreateContext( ovrEgl * egl, const ovrEgl * shareEgl )
{
	if ( egl->Display != 0 )
	{
		return;
	}

	egl->Display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
	ALOGV( "        eglInitialize( Display, &MajorVersion, &MinorVersion )" );
	eglInitialize( egl->Display, &egl->MajorVersion, &egl->MinorVersion );
	// Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
	// flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
	// settings, and that is completely wasted for our warp target.
	const int MAX_CONFIGS = 1024;
	EGLConfig configs[MAX_CONFIGS];
	EGLint numConfigs = 0;
	if ( eglGetConfigs( egl->Display, configs, MAX_CONFIGS, &numConfigs ) == EGL_FALSE )
	{
		ALOGE( "        eglGetConfigs() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	const EGLint configAttribs[] =
	{
		EGL_RED_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_BLUE_SIZE,		8,
		EGL_ALPHA_SIZE,		8, // need alpha for the multi-pass timewarp compositor
		EGL_DEPTH_SIZE,		0,
		EGL_STENCIL_SIZE,	0,
		EGL_SAMPLES,		0,
		EGL_NONE
	};
	egl->Config = 0;
	for ( int i = 0; i < numConfigs; i++ )
	{
		EGLint value = 0;

		eglGetConfigAttrib( egl->Display, configs[i], EGL_RENDERABLE_TYPE, &value );
		if ( ( value & EGL_OPENGL_ES3_BIT_KHR ) != EGL_OPENGL_ES3_BIT_KHR )
		{
			continue;
		}

		// The pbuffer config also needs to be compatible with normal window rendering
		// so it can share textures with the window context.
		eglGetConfigAttrib( egl->Display, configs[i], EGL_SURFACE_TYPE, &value );
		if ( ( value & ( EGL_WINDOW_BIT | EGL_PBUFFER_BIT ) ) != ( EGL_WINDOW_BIT | EGL_PBUFFER_BIT ) )
		{
			continue;
		}

		int	j = 0;
		for ( ; configAttribs[j] != EGL_NONE; j += 2 )
		{
			eglGetConfigAttrib( egl->Display, configs[i], configAttribs[j], &value );
			if ( value != configAttribs[j + 1] )
			{
				break;
			}
		}
		if ( configAttribs[j] == EGL_NONE )
		{
			egl->Config = configs[i];
			break;
		}
	}
	if ( egl->Config == 0 )
	{
		ALOGE( "        eglChooseConfig() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	EGLint contextAttribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	ALOGV( "        Context = eglCreateContext( Display, Config, EGL_NO_CONTEXT, contextAttribs )" );
	egl->Context = eglCreateContext( egl->Display, egl->Config, ( shareEgl != NULL ) ? shareEgl->Context : EGL_NO_CONTEXT, contextAttribs );
	if ( egl->Context == EGL_NO_CONTEXT )
	{
		ALOGE( "        eglCreateContext() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	const EGLint surfaceAttribs[] =
	{
		EGL_WIDTH, 16,
		EGL_HEIGHT, 16,
		EGL_NONE
	};
	ALOGV( "        TinySurface = eglCreatePbufferSurface( Display, Config, surfaceAttribs )" );
	egl->TinySurface = eglCreatePbufferSurface( egl->Display, egl->Config, surfaceAttribs );
	if ( egl->TinySurface == EGL_NO_SURFACE )
	{
		ALOGE( "        eglCreatePbufferSurface() failed: %s", EglErrorString( eglGetError() ) );
		eglDestroyContext( egl->Display, egl->Context );
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
	ALOGV( "        eglMakeCurrent( Display, TinySurface, TinySurface, Context )" );
	if ( eglMakeCurrent( egl->Display, egl->TinySurface, egl->TinySurface, egl->Context ) == EGL_FALSE )
	{
		ALOGE( "        eglMakeCurrent() failed: %s", EglErrorString( eglGetError() ) );
		eglDestroySurface( egl->Display, egl->TinySurface );
		eglDestroyContext( egl->Display, egl->Context );
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
}

static void ovrEgl_DestroyContext( ovrEgl * egl )
{
	if ( egl->Display != 0 )
	{
		ALOGE( "        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )" );
		if ( eglMakeCurrent( egl->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ) == EGL_FALSE )
		{
			ALOGE( "        eglMakeCurrent() failed: %s", EglErrorString( eglGetError() ) );
		}
	}
	if ( egl->Context != EGL_NO_CONTEXT )
	{
		ALOGE( "        eglDestroyContext( Display, Context )" );
		if ( eglDestroyContext( egl->Display, egl->Context ) == EGL_FALSE )
		{
			ALOGE( "        eglDestroyContext() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->Context = EGL_NO_CONTEXT;
	}
	if ( egl->TinySurface != EGL_NO_SURFACE )
	{
		ALOGE( "        eglDestroySurface( Display, TinySurface )" );
		if ( eglDestroySurface( egl->Display, egl->TinySurface ) == EGL_FALSE )
		{
			ALOGE( "        eglDestroySurface() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->TinySurface = EGL_NO_SURFACE;
	}
	if ( egl->Display != 0 )
	{
		ALOGE( "        eglTerminate( Display )" );
		if ( eglTerminate( egl->Display ) == EGL_FALSE )
		{
			ALOGE( "        eglTerminate() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->Display = 0;
	}
}

/*
================================================================================

ovrFramebuffer

================================================================================
*/


static void ovrFramebuffer_Clear( ovrFramebuffer * frameBuffer )
{
	frameBuffer->Width = 0;
	frameBuffer->Height = 0;
	frameBuffer->Multisamples = 0;
	frameBuffer->TextureSwapChainLength = 0;
	frameBuffer->TextureSwapChainIndex = 0;
	frameBuffer->ColorTextureSwapChain = NULL;
	frameBuffer->DepthBuffers = NULL;
	frameBuffer->FrameBuffers = NULL;
}

static bool ovrFramebuffer_Create( ovrFramebuffer * frameBuffer, const GLenum colorFormat, const int width, const int height, const int multisamples )
{
	PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT =
		(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress( "glRenderbufferStorageMultisampleEXT" );
	PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT =
		(PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress( "glFramebufferTexture2DMultisampleEXT" );

	PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR =
		(PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) eglGetProcAddress( "glFramebufferTextureMultiviewOVR" );
	PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR =
		(PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC) eglGetProcAddress( "glFramebufferTextureMultisampleMultiviewOVR" );

    frameBuffer->Width = width;
	frameBuffer->Height = height;
	frameBuffer->Multisamples = multisamples;

	frameBuffer->ColorTextureSwapChain = vrapi_CreateTextureSwapChain3( VRAPI_TEXTURE_TYPE_2D, colorFormat, frameBuffer->Width, frameBuffer->Height, 1, 3 );
	frameBuffer->TextureSwapChainLength = vrapi_GetTextureSwapChainLength( frameBuffer->ColorTextureSwapChain );
	frameBuffer->DepthBuffers = (GLuint *)malloc( frameBuffer->TextureSwapChainLength * sizeof( GLuint ) );
	frameBuffer->FrameBuffers = (GLuint *)malloc( frameBuffer->TextureSwapChainLength * sizeof( GLuint ) );

	for ( int i = 0; i < frameBuffer->TextureSwapChainLength; i++ )
	{
		// Create the color buffer texture.
		const GLuint colorTexture = vrapi_GetTextureSwapChainHandle( frameBuffer->ColorTextureSwapChain, i );
		GLenum colorTextureTarget = GL_TEXTURE_2D;
		GL( glBindTexture( colorTextureTarget, colorTexture ) );
		if ( glExtensions.EXT_texture_border_clamp )
		{
			GL( glTexParameteri( colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER ) );
			GL( glTexParameteri( colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER ) );
			GLfloat borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			GL( glTexParameterfv( colorTextureTarget, GL_TEXTURE_BORDER_COLOR, borderColor ) );
		}
		else
		{
        // Just clamp to edge. However, this requires manually clearing the border
        // around the layer to clear the edge texels.
        GL( glTexParameteri( colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
        GL( glTexParameteri( colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );
		}
		GL( glTexParameteri( colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
		GL( glTexParameteri( colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
		GL( glBindTexture( colorTextureTarget, 0 ) );

		{
			if ( multisamples > 1 && glRenderbufferStorageMultisampleEXT != NULL && glFramebufferTexture2DMultisampleEXT != NULL )
			{
				// Create multisampled depth buffer.
				GL( glGenRenderbuffers( 1, &frameBuffer->DepthBuffers[i] ) );
				GL( glBindRenderbuffer( GL_RENDERBUFFER, frameBuffer->DepthBuffers[i] ) );
				GL( glRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER, multisamples, GL_DEPTH_COMPONENT24, frameBuffer->Width, frameBuffer->Height ) );
				GL( glBindRenderbuffer( GL_RENDERBUFFER, 0 ) );

				// Create the frame buffer.
				// NOTE: glFramebufferTexture2DMultisampleEXT only works with GL_FRAMEBUFFER.
				GL( glGenFramebuffers( 1, &frameBuffer->FrameBuffers[i] ) );
				GL( glBindFramebuffer( GL_FRAMEBUFFER, frameBuffer->FrameBuffers[i] ) );
				GL( glFramebufferTexture2DMultisampleEXT( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0, multisamples ) );
				GL( glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, frameBuffer->DepthBuffers[i] ) );
				GL( GLenum renderFramebufferStatus = glCheckFramebufferStatus( GL_FRAMEBUFFER ) );
				GL( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
				if ( renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE )
				{
					ALOGE( "Incomplete frame buffer object: %s", GlFrameBufferStatusString( renderFramebufferStatus ) );
					return false;
				}
			}
			else
			{
				// Create depth buffer.
				GL( glGenRenderbuffers( 1, &frameBuffer->DepthBuffers[i] ) );
				GL( glBindRenderbuffer( GL_RENDERBUFFER, frameBuffer->DepthBuffers[i] ) );
				GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, frameBuffer->Width, frameBuffer->Height ) );
				GL( glBindRenderbuffer( GL_RENDERBUFFER, 0 ) );

				// Create the frame buffer.
				GL( glGenFramebuffers( 1, &frameBuffer->FrameBuffers[i] ) );
				GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[i] ) );
				GL( glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, frameBuffer->DepthBuffers[i] ) );
				GL( glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0 ) );
				GL( GLenum renderFramebufferStatus = glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) );
				GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 ) );
				if ( renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE )
				{
					ALOGE( "Incomplete frame buffer object: %s", GlFrameBufferStatusString( renderFramebufferStatus ) );
					return false;
				}
			}
		}
	}

	return true;
}

void ovrFramebuffer_Destroy( ovrFramebuffer * frameBuffer )
{
	GL( glDeleteFramebuffers( frameBuffer->TextureSwapChainLength, frameBuffer->FrameBuffers ) );
	GL( glDeleteRenderbuffers( frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers ) );

	vrapi_DestroyTextureSwapChain( frameBuffer->ColorTextureSwapChain );

	free( frameBuffer->DepthBuffers );
	free( frameBuffer->FrameBuffers );

	ovrFramebuffer_Clear( frameBuffer );
}

void ovrFramebuffer_SetCurrent( ovrFramebuffer * frameBuffer )
{
    GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[frameBuffer->TextureSwapChainIndex] ) );
}

void ovrFramebuffer_SetNone()
{
    GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 ) );
}

void ovrFramebuffer_Resolve( ovrFramebuffer * frameBuffer )
{
	// Discard the depth buffer, so the tiler won't need to write it back out to memory.
	const GLenum depthAttachment[1] = { GL_DEPTH_ATTACHMENT };
	glInvalidateFramebuffer( GL_DRAW_FRAMEBUFFER, 1, depthAttachment );

    // Flush this frame worth of commands.
    glFlush();
}

void ovrFramebuffer_Advance( ovrFramebuffer * frameBuffer )
{
	// Advance to the next texture from the set.
	frameBuffer->TextureSwapChainIndex = ( frameBuffer->TextureSwapChainIndex + 1 ) % frameBuffer->TextureSwapChainLength;
}


void ovrFramebuffer_ClearEdgeTexels( ovrFramebuffer * frameBuffer )
{
	GL( glEnable( GL_SCISSOR_TEST ) );
	GL( glViewport( 0, 0, frameBuffer->Width, frameBuffer->Height ) );

	// Explicitly clear the border texels to black because OpenGL-ES does not support GL_CLAMP_TO_BORDER.
	// Clear to fully opaque black.
	GL( glClearColor( 0.0f, 0.0f, 0.0f, 1.0f ) );

	// bottom
	GL( glScissor( 0, 0, frameBuffer->Width, 1 ) );
	GL( glClear( GL_COLOR_BUFFER_BIT ) );
	// top
	GL( glScissor( 0, frameBuffer->Height - 1, frameBuffer->Width, 1 ) );
	GL( glClear( GL_COLOR_BUFFER_BIT ) );
	// left
	GL( glScissor( 0, 0, 1, frameBuffer->Height ) );
	GL( glClear( GL_COLOR_BUFFER_BIT ) );
	// right
	GL( glScissor( frameBuffer->Width - 1, 0, 1, frameBuffer->Height ) );
	GL( glClear( GL_COLOR_BUFFER_BIT ) );


	GL( glScissor( 0, 0, 0, 0 ) );
	GL( glDisable( GL_SCISSOR_TEST ) );
}


/*
================================================================================

ovrRenderer

================================================================================
*/


void ovrRenderer_Clear( ovrRenderer * renderer )
{
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		ovrFramebuffer_Clear( &renderer->FrameBuffer[eye] );
	}

	renderer->NumBuffers = VRAPI_FRAME_LAYER_EYE_MAX;
}


void ovrRenderer_Create( int width, int height, ovrRenderer * renderer, const ovrJava * java )
{
	renderer->NumBuffers = VRAPI_FRAME_LAYER_EYE_MAX;

	// Create the render Textures.
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		ovrFramebuffer_Create( &renderer->FrameBuffer[eye],
							   GL_RGBA8,
							   width,
							   height,
							   NUM_MULTI_SAMPLES );
	}
}

void ovrRenderer_Destroy( ovrRenderer * renderer )
{
	for ( int eye = 0; eye < renderer->NumBuffers; eye++ )
	{
		ovrFramebuffer_Destroy( &renderer->FrameBuffer[eye] );
	}
}


#ifndef EPSILON
#define EPSILON 0.001f
#endif

static ovrVector3f normalizeVec(ovrVector3f vec) {
    //NOTE: leave w-component untouched
    //@@const float EPSILON = 0.000001f;
    float xxyyzz = vec.x*vec.x + vec.y*vec.y + vec.z*vec.z;
    //@@if(xxyyzz < EPSILON)
    //@@    return *this; // do nothing if it is zero vector

    //float invLength = invSqrt(xxyyzz);
    ovrVector3f result;
    float invLength = 1.0f / sqrtf(xxyyzz);
    result.x = vec.x * invLength;
    result.y = vec.y * invLength;
    result.z = vec.z * invLength;
    return result;
}

void NormalizeAngles(vec3_t angles)
{
	while (angles[0] >= 90) angles[0] -= 180;
	while (angles[1] >= 180) angles[1] -= 360;
	while (angles[2] >= 180) angles[2] -= 360;
	while (angles[0] < -90) angles[0] += 180;
	while (angles[1] < -180) angles[1] += 360;
	while (angles[2] < -180) angles[2] += 360;
}

void GetAnglesFromVectors(const ovrVector3f forward, const ovrVector3f right, const ovrVector3f up, vec3_t angles)
{
	float sr, sp, sy, cr, cp, cy;

	sp = -forward.z;

	float cp_x_cy = forward.x;
	float cp_x_sy = forward.y;
	float cp_x_sr = -right.z;
	float cp_x_cr = up.z;

	float yaw = atan2(cp_x_sy, cp_x_cy);
	float roll = atan2(cp_x_sr, cp_x_cr);

	cy = cos(yaw);
	sy = sin(yaw);
	cr = cos(roll);
	sr = sin(roll);

	if (fabs(cy) > EPSILON)
	{
	cp = cp_x_cy / cy;
	}
	else if (fabs(sy) > EPSILON)
	{
	cp = cp_x_sy / sy;
	}
	else if (fabs(sr) > EPSILON)
	{
	cp = cp_x_sr / sr;
	}
	else if (fabs(cr) > EPSILON)
	{
	cp = cp_x_cr / cr;
	}
	else
	{
	cp = cos(asin(sp));
	}

	float pitch = atan2(sp, cp);

	angles[0] = pitch / (M_PI*2.f / 360.f);
	angles[1] = yaw / (M_PI*2.f / 360.f);
	angles[2] = roll / (M_PI*2.f / 360.f);

	NormalizeAngles(angles);
}

void QuatToYawPitchRoll(ovrQuatf q, float pitchAdjust, vec3_t out) {

    ovrMatrix4f mat = ovrMatrix4f_CreateFromQuaternion( &q );

    if (pitchAdjust != 0.0f)
	{
		ovrMatrix4f rot = ovrMatrix4f_CreateRotation(radians(pitchAdjust), 0.0f, 0.0f);
		mat = ovrMatrix4f_Multiply(&mat, &rot);
	}

    ovrVector4f v1 = {0, 0, -1, 0};
    ovrVector4f v2 = {1, 0, 0, 0};
    ovrVector4f v3 = {0, 1, 0, 0};

    ovrVector4f forwardInVRSpace = ovrVector4f_MultiplyMatrix4f(&mat, &v1);
    ovrVector4f rightInVRSpace = ovrVector4f_MultiplyMatrix4f(&mat, &v2);
    ovrVector4f upInVRSpace = ovrVector4f_MultiplyMatrix4f(&mat, &v3);

	ovrVector3f forward = {-forwardInVRSpace.z, -forwardInVRSpace.x, forwardInVRSpace.y};
	ovrVector3f right = {-rightInVRSpace.z, -rightInVRSpace.x, rightInVRSpace.y};
	ovrVector3f up = {-upInVRSpace.z, -upInVRSpace.x, upInVRSpace.y};

	ovrVector3f forwardNormal = normalizeVec(forward);
	ovrVector3f rightNormal = normalizeVec(right);
	ovrVector3f upNormal = normalizeVec(up);

	GetAnglesFromVectors(forwardNormal, rightNormal, upNormal, out);
	return;
}

void setWorldPosition( float x, float y, float z )
{
    positionDeltaThisFrame[0] = (worldPosition[0] - x);
    positionDeltaThisFrame[1] = (worldPosition[1] - y);
    positionDeltaThisFrame[2] = (worldPosition[2] - z);

    worldPosition[0] = x;
    worldPosition[1] = y;
    worldPosition[2] = z;
}

extern float playerYaw;
void setHMDPosition( float x, float y, float z, float yaw )
{
	static bool s_useScreen = false;

	VectorSet(hmdPosition, x, y, z);

    if (s_useScreen != useScreenLayer())
    {
		s_useScreen = useScreenLayer();

		//Record player height on transition
        playerHeight = y;
    }

	if (!useScreenLayer())
    {
    	playerYaw = yaw;
	}
}

qboolean isMultiplayer()
{
	return Cvar_VariableValue("maxclients") > 1;
}


/*
========================
Android_Vibrate
========================
*/

//0 = left, 1 = right
float vibration_channel_duration[2] = {0.0f, 0.0f};
float vibration_channel_intensity[2] = {0.0f, 0.0f};

void Android_Vibrate( float duration, int channel, float intensity )
{
	if (vibration_channel_duration[channel] > 0.0f)
		return;

	if (vibration_channel_duration[channel] == -1.0f &&	duration != 0.0f)
		return;

	vibration_channel_duration[channel] = duration;
	vibration_channel_intensity[channel] = intensity;
}

static void ovrRenderer_RenderFrame( ovrRenderer * renderer, const ovrJava * java,
											const ovrTracking2 * tracking, ovrMobile * ovr )
{
    ovrTracking2 updatedTracking = *tracking;

    //Get orientation
    // We extract Yaw, Pitch, Roll instead of directly using the orientation
    // to allow "additional" yaw manipulation with mouse/controller.
    const ovrQuatf quatHmd = tracking->HeadPose.Pose.Orientation;
    const ovrVector3f positionHmd = tracking->HeadPose.Pose.Position;
    QuatToYawPitchRoll(quatHmd, 0.0f, hmdorientation);
    setHMDPosition(positionHmd.x, positionHmd.y, positionHmd.z, hmdorientation[YAW]);

    //Use hmd position for world position
    setWorldPosition(positionHmd.x, positionHmd.y, positionHmd.z);

    ALOGV("        HMD-Yaw: %f", hmdorientation[YAW]);
    ALOGV("        HMD-Position: %f, %f, %f", positionHmd.x, positionHmd.y, positionHmd.z);

    //Set move information - if showing menu, don't pass head orientation through
    if (m_state == m_none)
        QC_MoveEvent(hmdorientation[YAW], hmdorientation[PITCH], hmdorientation[ROLL]);
    else
        QC_MoveEvent(0, 0, 0);

    //Set everything up
    QC_BeginFrame(/* true to stop time if needed in future */ false);

	// Render the eye images.
	for ( int eye = 0; eye < renderer->NumBuffers; eye++ )
	{
		ovrFramebuffer * frameBuffer = &renderer->FrameBuffer[eye];

		ovrFramebuffer_SetCurrent( frameBuffer );

		//If showing the menu, then render mono, easier to navigate menu
		{
			GL( glEnable( GL_SCISSOR_TEST ) );
			GL( glDepthMask( GL_TRUE ) );
			GL( glEnable( GL_DEPTH_TEST ) );
			GL( glDepthFunc( GL_LEQUAL ) );

			//We are using the size of the render target
			GL( glViewport( 0, 0, frameBuffer->Width, frameBuffer->Height ) );
			GL( glScissor( 0, 0, frameBuffer->Width, frameBuffer->Height ) );

			GL( glClearColor( 0.01f, 0.0f, 0.0f, 1.0f ) );
			GL( glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) );
			GL( glDisable(GL_SCISSOR_TEST));

            //Now do the drawing for this eye
            QC_DrawFrame(eye, 0, 0);
		}

        //Clear edge to prevent smearing
        ovrFramebuffer_ClearEdgeTexels( frameBuffer );


		ovrFramebuffer_Resolve( frameBuffer );
		ovrFramebuffer_Advance( frameBuffer );
	}

    QC_EndFrame();

	ovrFramebuffer_SetNone();
}


/*
================================================================================

ovrRenderThread

================================================================================
*/


/*
================================================================================

ovrApp

================================================================================
*/

typedef struct
{
	ovrJava				Java;
	ovrEgl				Egl;
	ANativeWindow *		NativeWindow;
	bool				Resumed;
	ovrMobile *			Ovr;
    ovrScene			Scene;
	long long			FrameIndex;
	double 				DisplayTime;
	int					SwapInterval;
	int					CpuLevel;
	int					GpuLevel;
	int					MainThreadTid;
	int					RenderThreadTid;
	ovrLayer_Union2		Layers[ovrMaxLayerCount];
	int					LayerCount;
	ovrRenderer			Renderer;
} ovrApp;

static void ovrApp_Clear( ovrApp * app )
{
	app->Java.Vm = NULL;
	app->Java.Env = NULL;
	app->Java.ActivityObject = NULL;
	app->Ovr = NULL;
	app->FrameIndex = 1;
	app->DisplayTime = 0;
	app->SwapInterval = 1;
	app->CpuLevel = 2;
	app->GpuLevel = 2;
	app->MainThreadTid = 0;
	app->RenderThreadTid = 0;

	ovrEgl_Clear( &app->Egl );

    ovrScene_Clear( &app->Scene );
	ovrRenderer_Clear( &app->Renderer );
}

static void ovrApp_PushBlackFinal( ovrApp * app )
{
	int frameFlags = 0;
	frameFlags |= VRAPI_FRAME_FLAG_FLUSH | VRAPI_FRAME_FLAG_FINAL;

	ovrLayerProjection2 layer = vrapi_DefaultLayerBlackProjection2();
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

	const ovrLayerHeader2 * layers[] =
	{
		&layer.Header
	};

	ovrSubmitFrameDescription2 frameDesc = {};
	frameDesc.Flags = frameFlags;
	frameDesc.SwapInterval = 1;
	frameDesc.FrameIndex = app->FrameIndex;
	frameDesc.DisplayTime = app->DisplayTime;
	frameDesc.LayerCount = 1;
	frameDesc.Layers = layers;

	vrapi_SubmitFrame2( app->Ovr, &frameDesc );
}

static void ovrApp_HandleVrModeChanges( ovrApp * app )
{
	if ( app->Resumed != false && app->NativeWindow != NULL )
	{
		if ( app->Ovr == NULL )
		{
			ovrModeParms parms = vrapi_DefaultModeParms( &app->Java );
			// Must reset the FLAG_FULLSCREEN window flag when using a SurfaceView
			parms.Flags |= VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

			parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
			parms.Display = (size_t)app->Egl.Display;
			parms.WindowSurface = (size_t)app->NativeWindow;
			parms.ShareContext = (size_t)app->Egl.Context;

			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			ALOGV( "        vrapi_EnterVrMode()" );

			app->Ovr = vrapi_EnterVrMode( &parms );

			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			// If entering VR mode failed then the ANativeWindow was not valid.
			if ( app->Ovr == NULL )
			{
				ALOGE( "Invalid ANativeWindow!" );
				app->NativeWindow = NULL;
			}

			// Set performance parameters once we have entered VR mode and have a valid ovrMobile.
			if ( app->Ovr != NULL )
			{
                //AmmarkoV : Set our refresh rate..!
                ovrResult result = vrapi_SetDisplayRefreshRate(app->Ovr, selectedFramerate);
                if (result == ovrSuccess) { ALOGV("Changed refresh rate. %f Hz", selectedFramerate); } else
                                          { ALOGV("Failed to change refresh rate to 90Hz Result=%d",result); }

				vrapi_SetClockLevels( app->Ovr, app->CpuLevel, app->GpuLevel );

				ALOGV( "		vrapi_SetClockLevels( %d, %d )", app->CpuLevel, app->GpuLevel );

				vrapi_SetPerfThread( app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid );

				ALOGV( "		vrapi_SetPerfThread( MAIN, %d )", app->MainThreadTid );

				vrapi_SetPerfThread( app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid );

				ALOGV( "		vrapi_SetPerfThread( RENDERER, %d )", app->RenderThreadTid );
			}
		}
	}
	else
	{
		if ( app->Ovr != NULL )
		{
			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			ALOGV( "        vrapi_LeaveVrMode()" );

			vrapi_LeaveVrMode( app->Ovr );
			app->Ovr = NULL;

			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );
		}
	}
}

static void handleTrackedControllerButton(ovrInputStateTrackedRemote * trackedRemoteState, ovrInputStateTrackedRemote * prevTrackedRemoteState, uint32_t button, int key)
{
    if ((trackedRemoteState->Buttons & button) != (prevTrackedRemoteState->Buttons & button))
    {
        QC_KeyEvent((trackedRemoteState->Buttons & button) > 0 ? 1 : 0, key, 0);
    }
}

static void rotateAboutOrigin(float v1, float v2, float rotation, vec2_t out)
{
    vec3_t temp;
    temp[0] = v1;
    temp[1] = v2;

    vec3_t v;
    matrix4x4_t matrix;
    Matrix4x4_CreateFromQuakeEntity(&matrix, 0.0f, 0.0f, 0.0f, 0.0f, rotation, 0.0f, 1.0f);
    Matrix4x4_Transform(&matrix, temp, v);

    out[0] = v[0];
    out[1] = v[1];
}


static void rotateAboutOrigin2(vec3_t in, float pitch, float yaw, vec3_t out)
{
    vec3_t v;
    matrix4x4_t matrix;
    Matrix4x4_CreateFromQuakeEntity(&matrix, 0.0f, 0.0f, 0.0f, pitch, yaw, 0.0f, 1.0f);
    Matrix4x4_Transform(&matrix, in, v);
    Vector2Copy(out, v);
}

ovrInputStateTrackedRemote leftTrackedRemoteState_old;
ovrInputStateTrackedRemote leftTrackedRemoteState_new;
ovrTracking leftRemoteTracking;
ovrInputStateTrackedRemote rightTrackedRemoteState_old;
ovrInputStateTrackedRemote rightTrackedRemoteState_new;
ovrTracking rightRemoteTracking;

//Text Input stuff
bool textInput = false;
int shift = 0;
int left_grid = 0;
char left_lower[3][10] = {"bcfihgdae", "klorqpmjn", "tuwzyxvs "};
char left_shift[3][10] = {"BCFIHGDAE", "KLORQPMJN", "TUWZYXVS "};
int right_grid = 0;
char right_lower[3][10] = {"236987415", "+-)]&[(?0", { K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, 0}};
char right_shift[3][10] = {"\"*:|._~/#", "%^}>,<{\\@", { 0, K_F9, 0, K_F12, 0, K_F10, 0, K_F11, 0}};

char left_grid_map[2][3][3][9] = {
    {
        {
                "a  b  c", "j  k  l", "s  t  u"
        },
        {
                "d  e  f", "m  n  o", "v     w"
        },
        {
                "g  h  i", "p  q  r", "x  y  z"
        },
    },
    {
        {
                "A  B  C", "J  K  L", "S  T  U"
        },
        {
                "D  E  F", "M  N  O", "V     W"
        },
        {
                "G  H  I", "P  Q  R", "X  Y  Z"
        },

    }
};


char right_grid_map[2][3][3][9] = {
        {
                {
                        "1  2  3", "?  +  -", "F1 F2 F3"
                },
                {
                        "4  5  6", "(  0  )", "F8    F4"
                },
                {
                        "7  8  9", "[  &  ]", "F7 F6 F5"
                },
        },
        {
                {
                        "/  \"  *", "\\  %  ^", "   F9   "
                },
                {
                        "~  #  :", "{  @  }",   "F12  F10"
                },
                {
                        "_  .  |", "<  ,  >",    "  F11   "
                },
        }
};


static int getCharacter(float x, float y)
{
    int c = 8;
    if (x < -0.3f || x > 0.3f || y < -0.3f || y > 0.3f)
    {
        if (x == 0.0f)
        {
            if (y > 0.0f)
            {
                c = 0;
            }
            else
            {
                c = 4;
            }
        }
        else
        {
            float angle = atanf(y / x) / ((float)M_PI / 180.0f);
            if (x > 0.0f)
            {
                c = (int)(((90.0f - angle) + 22.5f) / 45.0f);
            }
            else
            {
                c = (int)(((90.0f - angle) + 22.5f) / 45.0f) + 4;
                if (c == 8)
                    c = 0;
            }
        }
    }

    return c;
}

int breakHere = 0;


#define NLF_DEADZONE 0.1
#define NLF_POWER 2.2

float nonLinearFilter(float in)
{
	float val = 0.0f;
	if (in > NLF_DEADZONE)
	{
		val = in;
		val -= NLF_DEADZONE;
		val /= (1.0f - NLF_DEADZONE);
		val = powf(val, NLF_POWER);
	}
	else if (in < -NLF_DEADZONE)
	{
		val = in;
		val += NLF_DEADZONE;
		val /= (1.0f - NLF_DEADZONE);
		val = -powf(fabsf(val), NLF_POWER);
	}

	return val;
}


float length(float x, float y)
{
	return sqrtf(powf(x, 2.0f) + powf(y, 2.0f));
}

static void weaponHaptics()
{
	if (useScreenLayer())
	{
		return;
	}

	qboolean isFirePressed = false;
	if (cl_righthanded.integer) {
		//Fire
		isFirePressed = rightTrackedRemoteState_new.Buttons & ovrButton_Trigger;
	} else {
		isFirePressed = leftTrackedRemoteState_new.Buttons & ovrButton_Trigger;
	}

	if (isFirePressed)
	{
		static double timeLastHaptic = 0;
		double timeNow = GetTimeInMilliSeconds();

		float hapticInterval = 0;
		float hapticLevel = 0;
		float hapticLength = 0;

		switch (cl.stats[STAT_ACTIVEWEAPON])
		{
			case IT_SHOTGUN:
			{
				hapticInterval = 500;
				hapticLevel = 0.7f;
				hapticLength = 150;
			}
			break;
			case IT_SUPER_SHOTGUN:
			{
				hapticInterval = 700;
				hapticLevel = 0.8f;
				hapticLength = 200;
			}
				break;
			case IT_NAILGUN:
			{
				hapticInterval = 100;
				hapticLevel = 0.6f;
				hapticLength = 50;
			}
				break;
			case IT_SUPER_NAILGUN:
			{
				hapticInterval = 80;
				hapticLevel = 0.9f;
				hapticLength = 50;
			}
				break;
			case IT_GRENADE_LAUNCHER:
			{
				hapticInterval = 600;
				hapticLevel = 0.7f;
				hapticLength = 100;
			}
				break;
			case IT_ROCKET_LAUNCHER:
			{
				hapticInterval = 800;
				hapticLevel = 1.0f;
				hapticLength = 300;
			}
				break;
			case IT_LIGHTNING:
			{
				hapticInterval = 100;
				hapticLevel = lhrandom(0.0, 0.8f);
				hapticLength = 80;
			}
				break;
			case IT_SUPER_LIGHTNING:
			{
				hapticInterval = 100;
				hapticLevel = lhrandom(0.3, 1.0f);
				hapticLength = 60;
			}
				break;
			case IT_AXE:
			{
				hapticInterval = 500;
				hapticLevel = 0.6f;
				hapticLength = 100;
			}
				break;
		}

		if ((timeNow - timeLastHaptic) > hapticInterval)
		{
			timeLastHaptic = timeNow;
			Android_Vibrate(hapticLength, cl_righthanded.integer ? 1 : 0, hapticLevel);
			if (weapon_stabilised)
            {
                Android_Vibrate(hapticLength, cl_righthanded.integer ? 0 : 1, hapticLevel);
            }
		}
	}
}

static void ovrApp_HandleInput( ovrApp * app )
{
    float remote_movementSideways = 0.0f;
    float remote_movementForward = 0.0f;
    float positional_movementSideways = 0.0f;
    float positional_movementForward = 0.0f;
    float controllerAngles[3];

    //The amount of yaw changed by controller
    float yawOffset = cl.viewangles[YAW] - hmdorientation[YAW];

	for ( int i = 0; ; i++ ) {
		ovrInputCapabilityHeader cap;
		ovrResult result = vrapi_EnumerateInputDevices(app->Ovr, i, &cap);
		if (result < 0) {
			break;
		}

		if (cap.Type == ovrControllerType_TrackedRemote) {
			ovrTracking remoteTracking;
			ovrInputStateTrackedRemote trackedRemoteState;
			trackedRemoteState.Header.ControllerType = ovrControllerType_TrackedRemote;
			result = vrapi_GetCurrentInputState(app->Ovr, cap.DeviceID, &trackedRemoteState.Header);

			if (result == ovrSuccess) {
				ovrInputTrackedRemoteCapabilities remoteCapabilities;
				remoteCapabilities.Header = cap;
				result = vrapi_GetInputDeviceCapabilities(app->Ovr, &remoteCapabilities.Header);

				result = vrapi_GetInputTrackingState(app->Ovr, cap.DeviceID, app->DisplayTime,
													 &remoteTracking);

				if (remoteCapabilities.ControllerCapabilities & ovrControllerCaps_RightHand) {
					rightTrackedRemoteState_new = trackedRemoteState;
					rightRemoteTracking = remoteTracking;
					controllerIDs[1] = cap.DeviceID;
				} else{
					leftTrackedRemoteState_new = trackedRemoteState;
					leftRemoteTracking = remoteTracking;
					controllerIDs[0] = cap.DeviceID;
				}
			}
		}
	}

    ovrInputStateTrackedRemote *dominantTrackedRemoteState = cl_righthanded.integer ? &rightTrackedRemoteState_new : &leftTrackedRemoteState_new;
    ovrInputStateTrackedRemote *dominantTrackedRemoteStateOld = cl_righthanded.integer ? &rightTrackedRemoteState_old : &leftTrackedRemoteState_old;
	ovrTracking *dominantRemoteTracking = cl_righthanded.integer ? &rightRemoteTracking : &leftRemoteTracking;
	ovrInputStateTrackedRemote *offHandTrackedRemoteState = !cl_righthanded.integer ? &rightTrackedRemoteState_new : &leftTrackedRemoteState_new;
	ovrInputStateTrackedRemote *offHandTrackedRemoteStateOld = !cl_righthanded.integer ? &rightTrackedRemoteState_old : &leftTrackedRemoteState_old;
	ovrTracking *offHandRemoteTracking = !cl_righthanded.integer ? &rightRemoteTracking : &leftRemoteTracking;

	if (textInput)
    {
        //Toggle text input
        if ((leftTrackedRemoteState_new.Buttons & ovrButton_Y) &&
            (leftTrackedRemoteState_new.Buttons & ovrButton_Y) !=
            (leftTrackedRemoteState_old.Buttons & ovrButton_Y)) {
            textInput = !textInput;
            SCR_CenterPrint("Text Input: Disabled");
        }

        int left_char_index = getCharacter(leftTrackedRemoteState_new.Joystick.x, leftTrackedRemoteState_new.Joystick.y);
        int right_char_index = getCharacter(rightTrackedRemoteState_new.Joystick.x, rightTrackedRemoteState_new.Joystick.y);

        //Toggle Shift
        if ((leftTrackedRemoteState_new.Buttons & ovrButton_X) &&
            (leftTrackedRemoteState_new.Buttons & ovrButton_X) !=
            (leftTrackedRemoteState_old.Buttons & ovrButton_X)) {
            shift = 1 - shift;
        }

        //Cycle Left Grid
        if ((leftTrackedRemoteState_new.Buttons & ovrButton_GripTrigger) &&
            (leftTrackedRemoteState_new.Buttons & ovrButton_GripTrigger) !=
            (leftTrackedRemoteState_old.Buttons & ovrButton_GripTrigger)) {
            left_grid = (++left_grid) % 3;
        }

        //Cycle Right Grid
        if ((rightTrackedRemoteState_new.Buttons & ovrButton_GripTrigger) &&
            (rightTrackedRemoteState_new.Buttons & ovrButton_GripTrigger) !=
            (rightTrackedRemoteState_old.Buttons & ovrButton_GripTrigger)) {
            right_grid = (++right_grid) % 3;
        }

        char left_char;
        char right_char;
        if (shift)
        {
            left_char = left_shift[left_grid][left_char_index];
            right_char = right_shift[right_grid][right_char_index];
        } else{
            left_char = left_lower[left_grid][left_char_index];
            right_char = right_lower[right_grid][right_char_index];
        }

        //Enter
        if ((rightTrackedRemoteState_new.Buttons & ovrButton_A) !=
            (rightTrackedRemoteState_old.Buttons & ovrButton_A)) {
            QC_KeyEvent((rightTrackedRemoteState_new.Buttons & ovrButton_A) > 0 ? 1 : 0, K_ENTER, 0);
        }

        //Delete
        if ((rightTrackedRemoteState_new.Buttons & ovrButton_B) !=
            (rightTrackedRemoteState_old.Buttons & ovrButton_B)) {
            QC_KeyEvent((rightTrackedRemoteState_new.Buttons & ovrButton_B) > 0 ? 1 : 0, K_BACKSPACE, 0);
        }

        //Use Left Character
        if ((leftTrackedRemoteState_new.Buttons & ovrButton_Trigger) !=
            (leftTrackedRemoteState_old.Buttons & ovrButton_Trigger)) {
            QC_KeyEvent((leftTrackedRemoteState_new.Buttons & ovrButton_Trigger) > 0 ? 1 : 0,
                        left_char, left_char);
        }

        //Use Right Character
        if ((rightTrackedRemoteState_new.Buttons & ovrButton_Trigger) !=
            (rightTrackedRemoteState_old.Buttons & ovrButton_Trigger)) {
            QC_KeyEvent((rightTrackedRemoteState_new.Buttons & ovrButton_Trigger) > 0 ? 1 : 0,
                        right_char, right_char);
        }

        //Menu button - could be on left or right controller
        handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old,
                                      ovrButton_Enter, K_ESCAPE);
		handleTrackedControllerButton(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old,
									  ovrButton_Enter, K_ESCAPE);


        if (textInput) {
            //Draw grid maps to screen
            char buffer[256];

            //Give the user an idea of what the buttons are
            dpsnprintf(buffer, 256,
                       " %s       %s\n %s       %s\n %s       %s\n\nText Input:   %c    %c",
                       left_grid_map[shift][0][left_grid], right_grid_map[shift][0][right_grid],
                       left_grid_map[shift][1][left_grid], right_grid_map[shift][1][right_grid],
                       left_grid_map[shift][2][left_grid], right_grid_map[shift][2][right_grid],
                       left_char, right_char);
            SCR_CenterPrint(buffer);
        }

        //Save state
        leftTrackedRemoteState_old = leftTrackedRemoteState_new;
        rightTrackedRemoteState_old = rightTrackedRemoteState_new;

    } else {
		float distance = sqrtf(powf(offHandRemoteTracking->HeadPose.Pose.Position.x - dominantRemoteTracking->HeadPose.Pose.Position.x, 2) +
							   powf(offHandRemoteTracking->HeadPose.Pose.Position.y - dominantRemoteTracking->HeadPose.Pose.Position.y, 2) +
							   powf(offHandRemoteTracking->HeadPose.Pose.Position.z - dominantRemoteTracking->HeadPose.Pose.Position.z, 2));

        //dominant hand stuff first
        weapon_stabilised = distance < 0.5f &&
                (offHandTrackedRemoteState->Buttons & ovrButton_GripTrigger) &&
                cl.stats[STAT_ACTIVEWEAPON] != IT_AXE;

        {
            weaponOffset[0] = dominantRemoteTracking->HeadPose.Pose.Position.x - hmdPosition[0];
            weaponOffset[1] = dominantRemoteTracking->HeadPose.Pose.Position.y - hmdPosition[1];
            weaponOffset[2] = dominantRemoteTracking->HeadPose.Pose.Position.z - hmdPosition[2];

            weaponVelocity[0] = dominantRemoteTracking->HeadPose.LinearVelocity.x;
            weaponVelocity[1] = dominantRemoteTracking->HeadPose.LinearVelocity.y;
            weaponVelocity[2] = dominantRemoteTracking->HeadPose.LinearVelocity.z;

            ///Weapon location relative to view
            vec2_t v;
            rotateAboutOrigin(weaponOffset[0], weaponOffset[2], -yawOffset, v);
            weaponOffset[0] = v[0];
            weaponOffset[2] = v[1];

            //Set gun angles
            const ovrQuatf quatRemote = dominantRemoteTracking->HeadPose.Pose.Orientation;
            QuatToYawPitchRoll(quatRemote, vr_weaponpitchadjust.value, gunangles);

            if (weapon_stabilised)
            {
                float z = offHandRemoteTracking->HeadPose.Pose.Position.z - dominantRemoteTracking->HeadPose.Pose.Position.z;
                float x = offHandRemoteTracking->HeadPose.Pose.Position.x - dominantRemoteTracking->HeadPose.Pose.Position.x;
                float y = offHandRemoteTracking->HeadPose.Pose.Position.y - dominantRemoteTracking->HeadPose.Pose.Position.y;
                float zxDist = length(x, z);

                if (zxDist != 0.0f && z != 0.0f) {
                    VectorSet(gunangles, -degrees(atanf(y / zxDist)),  -degrees(atan2f(x, -z)), gunangles[ROLL]);
                }
            }

            gunangles[YAW] += yawOffset;

            //Change laser sight on joystick click
            if ((dominantTrackedRemoteState->Buttons & ovrButton_Joystick) &&
                (dominantTrackedRemoteState->Buttons & ovrButton_Joystick) !=
                (dominantTrackedRemoteStateOld->Buttons & ovrButton_Joystick)) {
                Cvar_SetValueQuick(&r_lasersight, (r_lasersight.integer + 1) % 3);
            }
        }

        //off-hand stuff
        float controllerYawHeading;
        float hmdYawHeading;
        {
            QuatToYawPitchRoll(offHandRemoteTracking->HeadPose.Pose.Orientation, 0.0f,
                               controllerAngles);

            controllerYawHeading = controllerAngles[YAW] - gunangles[YAW] + yawOffset;
            hmdYawHeading = hmdorientation[YAW] - gunangles[YAW] + yawOffset;
        }

        //Right-hand specific stuff
        {
            ALOGE("        Right-Controller-Position: %f, %f, %f",
                  rightRemoteTracking.HeadPose.Pose.Position.x,
                  rightRemoteTracking.HeadPose.Pose.Position.y,
                  rightRemoteTracking.HeadPose.Pose.Position.z);

            //This section corrects for the fact that the controller actually controls direction of movement, but we want to move relative to the direction the
            //player is facing for positional tracking
            float multiplier = (float)(2300.0f * (selectedFramerate / 72.0) ) /
                               (cl_movementspeed.value * ((offHandTrackedRemoteState->Buttons & ovrButton_Trigger) ? cl_movespeedkey.value : 1.0f));

            vec2_t v;
            rotateAboutOrigin(-positionDeltaThisFrame[0] * multiplier,
                              positionDeltaThisFrame[2] * multiplier, yawOffset - gunangles[YAW], v);
            positional_movementSideways = v[0];
            positional_movementForward = v[1];


            long t = Sys_Milliseconds();
            delta = t - oldtime;
            oldtime = t;
            if (delta > 1000)
                delta = 1000;
            QC_MotionEvent(delta, rightTrackedRemoteState_new.Joystick.x,
                           rightTrackedRemoteState_new.Joystick.y);

            if (bigScreen != 0) {

                int rightJoyState = (rightTrackedRemoteState_new.Joystick.x > 0.7f ? 1 : 0);
                if (rightJoyState != (rightTrackedRemoteState_old.Joystick.x > 0.7f ? 1 : 0)) {
                    QC_KeyEvent(rightJoyState, 'd', 0);
                }
                rightJoyState = (rightTrackedRemoteState_new.Joystick.x < -0.7f ? 1 : 0);
                if (rightJoyState != (rightTrackedRemoteState_old.Joystick.x < -0.7f ? 1 : 0)) {
                    QC_KeyEvent(rightJoyState, 'a', 0);
                }
                rightJoyState = (rightTrackedRemoteState_new.Joystick.y < -0.7f ? 1 : 0);
                if (rightJoyState != (rightTrackedRemoteState_old.Joystick.y < -0.7f ? 1 : 0)) {
                    QC_KeyEvent(rightJoyState, K_DOWNARROW, 0);
                }
                rightJoyState = (rightTrackedRemoteState_new.Joystick.y > 0.7f ? 1 : 0);
                if (rightJoyState != (rightTrackedRemoteState_old.Joystick.y > 0.7f ? 1 : 0)) {
                    QC_KeyEvent(rightJoyState, K_UPARROW, 0);
                }

                //Click an option
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old, ovrButton_A, K_ENTER);

                //Back button
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old, ovrButton_B, K_ESCAPE);
            } else {
                //Jump
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old, ovrButton_A, K_SPACE);

				//Adjust weapon aim pitch
				if ((rightTrackedRemoteState_new.Buttons & ovrButton_B) &&
					(rightTrackedRemoteState_new.Buttons & ovrButton_B) !=
					(rightTrackedRemoteState_old.Buttons & ovrButton_B)) {

					//Unused
				}

				{
					//Weapon/Inventory Chooser
					int rightJoyState = (rightTrackedRemoteState_new.Joystick.y < -0.7f ? 1 : 0);
					if (rightJoyState != (rightTrackedRemoteState_old.Joystick.y < -0.7f ? 1 : 0)) {
						QC_KeyEvent(rightJoyState, '/', 0);
					}
					rightJoyState = (rightTrackedRemoteState_new.Joystick.y > 0.7f ? 1 : 0);
					if (rightJoyState != (rightTrackedRemoteState_old.Joystick.y > 0.7f ? 1 : 0)) {
						QC_KeyEvent(rightJoyState, '#', 0);
					}
				}
            }

            if (cl_righthanded.integer) {
                //Fire
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old,
                                              ovrButton_Trigger, K_MOUSE1);
            } else {
                //Run
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old,
                                              ovrButton_Trigger, K_SHIFT);
            }

            rightTrackedRemoteState_old = rightTrackedRemoteState_new;

        }

        //Left-hand specific stuff
        {
            ALOGE("        Left-Controller-Position: %f, %f, %f",
                  leftRemoteTracking.HeadPose.Pose.Position.x,
                  leftRemoteTracking.HeadPose.Pose.Position.y,
                  leftRemoteTracking.HeadPose.Pose.Position.z);

            //Menu button
            handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old,
                                          ovrButton_Enter, K_ESCAPE);

            if (bigScreen != 0) {
                int leftJoyState = (leftTrackedRemoteState_new.Joystick.x > 0.7f ? 1 : 0);
                if (leftJoyState != (leftTrackedRemoteState_old.Joystick.x > 0.7f ? 1 : 0)) {
                    QC_KeyEvent(leftJoyState, 'd', 0);
                }
                leftJoyState = (leftTrackedRemoteState_new.Joystick.x < -0.7f ? 1 : 0);
                if (leftJoyState != (leftTrackedRemoteState_old.Joystick.x < -0.7f ? 1 : 0)) {
                    QC_KeyEvent(leftJoyState, 'a', 0);
                }
                leftJoyState = (leftTrackedRemoteState_new.Joystick.y < -0.7f ? 1 : 0);
                if (leftJoyState != (leftTrackedRemoteState_old.Joystick.y < -0.7f ? 1 : 0)) {
                    QC_KeyEvent(leftJoyState, K_DOWNARROW, 0);
                }
                leftJoyState = (leftTrackedRemoteState_new.Joystick.y > 0.7f ? 1 : 0);
                if (leftJoyState != (leftTrackedRemoteState_old.Joystick.y > 0.7f ? 1 : 0)) {
                    QC_KeyEvent(leftJoyState, K_UPARROW, 0);
                }
            }


			//Apply a filter and quadratic scaler so small movements are easier to make
			//and we don't get movement jitter when the joystick doesn't quite center properly
			float dist = length(leftTrackedRemoteState_new.Joystick.x, leftTrackedRemoteState_new.Joystick.y);
			float nlf = nonLinearFilter(dist);
			float x = nlf * leftTrackedRemoteState_new.Joystick.x;
			float y = nlf * leftTrackedRemoteState_new.Joystick.y;

            //Adjust to be off-hand controller oriented
            vec2_t v;
            rotateAboutOrigin(x,
                              y,
                              cl_walkdirection.integer == 1 ? hmdYawHeading : controllerYawHeading,
                              v);
            remote_movementSideways = v[0];
            remote_movementForward = v[1];

            if (cl_righthanded.integer) {
                //Run
                handleTrackedControllerButton(&leftTrackedRemoteState_new,
                                              &leftTrackedRemoteState_old,
                                              ovrButton_Trigger, K_SHIFT);
            } else {
                //Fire
                handleTrackedControllerButton(&leftTrackedRemoteState_new,
                                              &leftTrackedRemoteState_old,
                                              ovrButton_Trigger, K_MOUSE1);
            }

            static bool canUseQuickSave = false;
            if (canUseQuickSave)
            {
                if ((leftTrackedRemoteState_new.Buttons & ovrButton_X) &&
                    (leftTrackedRemoteState_new.Buttons & ovrButton_X) !=
                    (leftTrackedRemoteState_old.Buttons & ovrButton_X)) {
                    Cbuf_InsertText("save quick\n");

                    //Vibrate to let user know they successfully saved
					SCR_CenterPrint("Quick Saved");
                    Android_Vibrate(500, cl_righthanded.integer ? 0 : 1, 1.0);
                }

                if ((leftTrackedRemoteState_new.Buttons & ovrButton_Y) &&
                    (leftTrackedRemoteState_new.Buttons & ovrButton_Y) !=
                    (leftTrackedRemoteState_old.Buttons & ovrButton_Y)) {
                    Cbuf_InsertText("load quick");
                }
            }
            else {
#ifndef NDEBUG
                //Give all weapons and all ammo and god mode
                if ((leftTrackedRemoteState_new.Buttons & ovrButton_X) &&
                    (leftTrackedRemoteState_new.Buttons & ovrButton_X) !=
                    (leftTrackedRemoteState_old.Buttons & ovrButton_X)) {
                    Cbuf_InsertText("God\n");
                    Cbuf_InsertText("Impulse 9\n");
                    breakHere = 1;
                }
#endif

                //Toggle text input
                if ((leftTrackedRemoteState_new.Buttons & ovrButton_Y) &&
                    (leftTrackedRemoteState_new.Buttons & ovrButton_Y) !=
                    (leftTrackedRemoteState_old.Buttons & ovrButton_Y)) {
                    textInput = !textInput;
                }
            }

            if (offHandRemoteTracking->Status & (VRAPI_TRACKING_STATUS_POSITION_TRACKED | VRAPI_TRACKING_STATUS_POSITION_VALID)) {
                canUseQuickSave = false;
            }
            else if (!canUseQuickSave) {
                canUseQuickSave = true;

                //Vibrate to let user know they can quick save
                Android_Vibrate(500, cl_righthanded.integer ? 0 : 1, 1.0);
            }


            leftTrackedRemoteState_old = leftTrackedRemoteState_new;
        }

        QC_Analog(true, remote_movementSideways + positional_movementSideways,
                  remote_movementForward + positional_movementForward);


	    if (bullettime.integer)
        {
            float speed = powf(sqrtf(powf(leftTrackedRemoteState_new.Joystick.x, 2) + powf(leftTrackedRemoteState_new.Joystick.y, 2)), 1.1f);
            float movement = sqrtf(powf(positionDeltaThisFrame[0] * 80.0f, 2) + powf(positionDeltaThisFrame[1] * 80.0f, 2) + powf(positionDeltaThisFrame[2] * 80.0f, 2));
            float weaponMovement = sqrtf(powf(weaponVelocity[0], 2) + powf(weaponVelocity[1], 2) + powf(weaponVelocity[2], 2));

            float maximum = max(max(speed, movement), weaponMovement);

            speed = bound(0.12f, maximum, 1.0f);
            Cvar_SetValueQuick(&slowmo, speed);
        }
    }
}

/*
================================================================================

ovrMessageQueue

================================================================================
*/

typedef enum
{
	MQ_WAIT_NONE,		// don't wait
	MQ_WAIT_RECEIVED,	// wait until the consumer thread has received the message
	MQ_WAIT_PROCESSED	// wait until the consumer thread has processed the message
} ovrMQWait;

#define MAX_MESSAGE_PARMS	8
#define MAX_MESSAGES		1024

typedef struct
{
	int			Id;
	ovrMQWait	Wait;
	long long	Parms[MAX_MESSAGE_PARMS];
} ovrMessage;

static void ovrMessage_Init( ovrMessage * message, const int id, const int wait )
{
	message->Id = id;
	message->Wait = wait;
	memset( message->Parms, 0, sizeof( message->Parms ) );
}

static void		ovrMessage_SetPointerParm( ovrMessage * message, int index, void * ptr ) { *(void **)&message->Parms[index] = ptr; }
static void *	ovrMessage_GetPointerParm( ovrMessage * message, int index ) { return *(void **)&message->Parms[index]; }
static void		ovrMessage_SetIntegerParm( ovrMessage * message, int index, int value ) { message->Parms[index] = value; }
static int		ovrMessage_GetIntegerParm( ovrMessage * message, int index ) { return (int)message->Parms[index]; }
static void		ovrMessage_SetFloatParm( ovrMessage * message, int index, float value ) { *(float *)&message->Parms[index] = value; }
static float	ovrMessage_GetFloatParm( ovrMessage * message, int index ) { return *(float *)&message->Parms[index]; }

// Cyclic queue with messages.
typedef struct
{
	ovrMessage	 		Messages[MAX_MESSAGES];
	volatile int		Head;	// dequeue at the head
	volatile int		Tail;	// enqueue at the tail
	ovrMQWait			Wait;
	volatile bool		EnabledFlag;
	volatile bool		PostedFlag;
	volatile bool		ReceivedFlag;
	volatile bool		ProcessedFlag;
	pthread_mutex_t		Mutex;
	pthread_cond_t		PostedCondition;
	pthread_cond_t		ReceivedCondition;
	pthread_cond_t		ProcessedCondition;
} ovrMessageQueue;

static void ovrMessageQueue_Create( ovrMessageQueue * messageQueue )
{
	messageQueue->Head = 0;
	messageQueue->Tail = 0;
	messageQueue->Wait = MQ_WAIT_NONE;
	messageQueue->EnabledFlag = false;
	messageQueue->PostedFlag = false;
	messageQueue->ReceivedFlag = false;
	messageQueue->ProcessedFlag = false;

	pthread_mutexattr_t	attr;
	pthread_mutexattr_init( &attr );
	pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK );
	pthread_mutex_init( &messageQueue->Mutex, &attr );
	pthread_mutexattr_destroy( &attr );
	pthread_cond_init( &messageQueue->PostedCondition, NULL );
	pthread_cond_init( &messageQueue->ReceivedCondition, NULL );
	pthread_cond_init( &messageQueue->ProcessedCondition, NULL );
}

static void ovrMessageQueue_Destroy( ovrMessageQueue * messageQueue )
{
	pthread_mutex_destroy( &messageQueue->Mutex );
	pthread_cond_destroy( &messageQueue->PostedCondition );
	pthread_cond_destroy( &messageQueue->ReceivedCondition );
	pthread_cond_destroy( &messageQueue->ProcessedCondition );
}

static void ovrMessageQueue_Enable( ovrMessageQueue * messageQueue, const bool set )
{
	messageQueue->EnabledFlag = set;
}

static void ovrMessageQueue_PostMessage( ovrMessageQueue * messageQueue, const ovrMessage * message )
{
	if ( !messageQueue->EnabledFlag )
	{
		return;
	}
	while ( messageQueue->Tail - messageQueue->Head >= MAX_MESSAGES )
	{
		usleep( 1000 );
	}
	pthread_mutex_lock( &messageQueue->Mutex );
	messageQueue->Messages[messageQueue->Tail & ( MAX_MESSAGES - 1 )] = *message;
	messageQueue->Tail++;
	messageQueue->PostedFlag = true;
	pthread_cond_broadcast( &messageQueue->PostedCondition );
	if ( message->Wait == MQ_WAIT_RECEIVED )
	{
		while ( !messageQueue->ReceivedFlag )
		{
			pthread_cond_wait( &messageQueue->ReceivedCondition, &messageQueue->Mutex );
		}
		messageQueue->ReceivedFlag = false;
	}
	else if ( message->Wait == MQ_WAIT_PROCESSED )
	{
		while ( !messageQueue->ProcessedFlag )
		{
			pthread_cond_wait( &messageQueue->ProcessedCondition, &messageQueue->Mutex );
		}
		messageQueue->ProcessedFlag = false;
	}
	pthread_mutex_unlock( &messageQueue->Mutex );
}

static void ovrMessageQueue_SleepUntilMessage( ovrMessageQueue * messageQueue )
{
	if ( messageQueue->Wait == MQ_WAIT_PROCESSED )
	{
		messageQueue->ProcessedFlag = true;
		pthread_cond_broadcast( &messageQueue->ProcessedCondition );
		messageQueue->Wait = MQ_WAIT_NONE;
	}
	pthread_mutex_lock( &messageQueue->Mutex );
	if ( messageQueue->Tail > messageQueue->Head )
	{
		pthread_mutex_unlock( &messageQueue->Mutex );
		return;
	}
	while ( !messageQueue->PostedFlag )
	{
		pthread_cond_wait( &messageQueue->PostedCondition, &messageQueue->Mutex );
	}
	messageQueue->PostedFlag = false;
	pthread_mutex_unlock( &messageQueue->Mutex );
}

static bool ovrMessageQueue_GetNextMessage( ovrMessageQueue * messageQueue, ovrMessage * message, bool waitForMessages )
{
	if ( messageQueue->Wait == MQ_WAIT_PROCESSED )
	{
		messageQueue->ProcessedFlag = true;
		pthread_cond_broadcast( &messageQueue->ProcessedCondition );
		messageQueue->Wait = MQ_WAIT_NONE;
	}
	if ( waitForMessages )
	{
		ovrMessageQueue_SleepUntilMessage( messageQueue );
	}
	pthread_mutex_lock( &messageQueue->Mutex );
	if ( messageQueue->Tail <= messageQueue->Head )
	{
		pthread_mutex_unlock( &messageQueue->Mutex );
		return false;
	}
	*message = messageQueue->Messages[messageQueue->Head & ( MAX_MESSAGES - 1 )];
	messageQueue->Head++;
	pthread_mutex_unlock( &messageQueue->Mutex );
	if ( message->Wait == MQ_WAIT_RECEIVED )
	{
		messageQueue->ReceivedFlag = true;
		pthread_cond_broadcast( &messageQueue->ReceivedCondition );
	}
	else if ( message->Wait == MQ_WAIT_PROCESSED )
	{
		messageQueue->Wait = MQ_WAIT_PROCESSED;
	}
	return true;
}

/*
================================================================================

ovrAppThread

================================================================================
*/

enum
{
	MESSAGE_ON_CREATE,
	MESSAGE_ON_START,
	MESSAGE_ON_RESUME,
	MESSAGE_ON_PAUSE,
	MESSAGE_ON_STOP,
	MESSAGE_ON_DESTROY,
	MESSAGE_ON_SURFACE_CREATED,
	MESSAGE_ON_SURFACE_DESTROYED
};

typedef struct
{
	JavaVM *		JavaVm;
	jobject			ActivityObject;
	jclass          ActivityClass;
	pthread_t		Thread;
	ovrMessageQueue	MessageQueue;
	ANativeWindow * NativeWindow;
} ovrAppThread;

long shutdownCountdown;

int m_width;
int m_height;
static ovrJava java;

qboolean R_SetMode( void );

void Quest_GetScreenRes(int *width, int *height)
{
	*width = m_width;
	*height = m_height;
}

int Quest_GetRefresh()
{
    return vrapi_GetSystemPropertyInt( &java, VRAPI_SYS_PROP_DISPLAY_REFRESH_RATE );
}

float getFOV()
{
    return vrapi_GetSystemPropertyFloat( &java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_Y );
}

void Quest_MessageBox(const char *title, const char *text)
{
    ALOGE("%s %s", title, text);
}

float GetFOV()
{
    return vrapi_GetSystemPropertyFloat( &java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_Y );
}

float GetSysTicrate()
{
    return 1.0F / (float)(vrapi_GetSystemPropertyInt(&java, VRAPI_SYS_PROP_DISPLAY_REFRESH_RATE));
}

void * AppThreadFunction( void * parm )
{
	ovrAppThread * appThread = (ovrAppThread *)parm;

	java.Vm = appThread->JavaVm;
	(*java.Vm)->AttachCurrentThread( java.Vm, &java.Env, NULL );
	java.ActivityObject = appThread->ActivityObject;

	// Note that AttachCurrentThread will reset the thread name.
	prctl( PR_SET_NAME, (long)"OVR::Main", 0, 0, 0 );

	const ovrInitParms initParms = vrapi_DefaultInitParms( &java );
	int32_t initResult = vrapi_Initialize( &initParms );
	if ( initResult != VRAPI_INITIALIZE_SUCCESS )
	{
		// If intialization failed, vrapi_* function calls will not be available.
		exit( 0 );
	}

	ovrApp appState;
	ovrApp_Clear( &appState );
	appState.Java = java;

	// This app will handle android gamepad events itself.
	vrapi_SetPropertyInt( &appState.Java, VRAPI_EAT_NATIVE_GAMEPAD_EVENTS, 0 );

	//Using a symmetrical render target
    m_width=vrapi_GetSystemPropertyInt( &java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH ) * SS_MULTIPLIER;
    m_height=m_width;
    //AmmarkoV : Query Refresh rates and select maximum..!
    //----------------------------------------------------------------------------------------------------------- 
    int numberOfRefreshRates = vrapi_GetSystemPropertyInt(&java,VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
    float refreshRatesArray[16];
    if (numberOfRefreshRates > 16 ) { numberOfRefreshRates = 16; }
    vrapi_GetSystemPropertyFloatArray(&java, VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES,&refreshRatesArray[0], numberOfRefreshRates);
    bool foundRefresh = false;
    for (int i = 0; i < numberOfRefreshRates; i++)
    {
    	//Select the max framerate
        if (selectedFramerate < refreshRatesArray[i])
         {
             selectedFramerate = refreshRatesArray[i];
         }

        //If users supplied refresh on the command line make sure it is one of the valid ones
        if (REFRESH == refreshRatesArray[i])
        {
            foundRefresh = true;
        }
    }

    //User supplied invalid (or didn't set it)
    if (!foundRefresh)
    {
        REFRESH = -1;
    }

    if (REFRESH == -1) {
    	//Cap to 90fps for Quest 2
        if (selectedFramerate > 90.0) {
            ALOGV("Soft limiting to 90.0 Hz as per John carmack's request ( https://www.onlinepeeps.org/oculus-quest-2-according-to-carmack-in-the-future-also-at-120-hz/ );P");
            selectedFramerate = 90.0;
        }
    }
    else
    {
        selectedFramerate = REFRESH;
    }

    //-----------------------------------------------------------------------------------------------------------


	ovrEgl_CreateContext( &appState.Egl, NULL );

	EglInitExtensions();

	appState.CpuLevel = CPU_LEVEL;
	appState.GpuLevel = GPU_LEVEL;
	appState.MainThreadTid = gettid();

	ovrRenderer_Create( m_width, m_height, &appState.Renderer, &java );

    chdir("/sdcard/QuakeQuest");

    hmdType = vrapi_GetSystemPropertyInt(&java, VRAPI_SYS_PROP_DEVICE_TYPE);

    for ( bool destroyed = false; destroyed == false; )
	{
		for ( ; ; )
		{
			ovrMessage message;
			const bool waitForMessages = ( appState.Ovr == NULL && destroyed == false );
			if ( !ovrMessageQueue_GetNextMessage( &appThread->MessageQueue, &message, waitForMessages ) )
			{
				break;
			}

			switch ( message.Id )
			{
				case MESSAGE_ON_CREATE:
				{
					break;
				}
				case MESSAGE_ON_START:
				{
					if (!quake_initialised)
					{
						ALOGV( "    Initialising Quake Engine" );

                        QC_SetResolution((int)m_width, (int)m_height);

						if (argc != 0)
						{
							main(argc, argv);
						}
						else
						{
							int argc = 1; char *argv[] = { "quake" };
							main(argc, argv);
						}

						quake_initialised = true;

						//Ensure game starts with credits active
						MR_ToggleMenu(2);
					}
					break;
				}
				case MESSAGE_ON_RESUME:
				{
					//If we get here, then user has opted not to quit
					appState.Resumed = true;
					break;
				}
				case MESSAGE_ON_PAUSE:
				{
					appState.Resumed = false;
					break;
				}
				case MESSAGE_ON_STOP:
				{
					break;
				}
				case MESSAGE_ON_DESTROY:
				{
					appState.NativeWindow = NULL;
					destroyed = true;
					break;
				}
				case MESSAGE_ON_SURFACE_CREATED:	{ appState.NativeWindow = (ANativeWindow *)ovrMessage_GetPointerParm( &message, 0 ); break; }
				case MESSAGE_ON_SURFACE_DESTROYED:	{ appState.NativeWindow = NULL; break; }
			}

			ovrApp_HandleVrModeChanges( &appState );
		}

		if ( appState.Ovr == NULL )
		{
			continue;
		}

        //Use floor based tracking space
        vrapi_SetTrackingSpace(appState.Ovr, VRAPI_TRACKING_SPACE_LOCAL_FLOOR);

		// Create the scene if not yet created.
		// The scene is created here to be able to show a loading icon.
		if ( !ovrScene_IsCreated( &appState.Scene ) )
		{
			ovrScene_Create( m_width, m_height, &appState.Scene, &java );
		}

        // This is the only place the frame index is incremented, right before
        // calling vrapi_GetPredictedDisplayTime().
        appState.FrameIndex++;

        // Create the scene if not yet created.
		// The scene is created here to be able to show a loading icon.
		if (!quake_initialised || runStatus != -1)
		{
			// Show a loading icon.
			int frameFlags = 0;
			frameFlags |= VRAPI_FRAME_FLAG_FLUSH;

			ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
			blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

			ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
			iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

			const ovrLayerHeader2 * layers[] =
			{
				&blackLayer.Header,
				&iconLayer.Header,
			};

			ovrSubmitFrameDescription2 frameDesc = {};
			frameDesc.Flags = frameFlags;
			frameDesc.SwapInterval = 1;
			frameDesc.FrameIndex = appState.FrameIndex;
			frameDesc.DisplayTime = appState.DisplayTime;
			frameDesc.LayerCount = 2;
			frameDesc.Layers = layers;

			vrapi_SubmitFrame2( appState.Ovr, &frameDesc );
		}

		//Handle haptics
		static float lastFrameTime = 0.0f;
		float timestamp = (float)(GetTimeInMilliSeconds());
		float frametime = timestamp - lastFrameTime;
		lastFrameTime = timestamp;

		for (int i = 0; i < 2; ++i) {
			if (vibration_channel_duration[i] > 0.0f ||
				vibration_channel_duration[i] == -1.0f) {
				vrapi_SetHapticVibrationSimple(appState.Ovr, controllerIDs[i],
											   vibration_channel_intensity[i]);

				if (vibration_channel_duration[i] != -1.0f) {
					vibration_channel_duration[i] -= frametime;

					if (vibration_channel_duration[i] < 0.0f) {
						vibration_channel_duration[i] = 0.0f;
						vibration_channel_intensity[i] = 0.0f;
					}
				}
			} else {
				vrapi_SetHapticVibrationSimple(appState.Ovr, controllerIDs[i], 0.0f);
			}
		}

        if (runStatus == -1) {
#ifndef NDEBUG
            if (appState.FrameIndex > 10800)
            {
                //Trigger shutdown after a couple of minutes in debug mode
                runStatus = 0;
            }
#endif

            if (hmdType == VRAPI_DEVICE_TYPE_OCULUSQUEST2) {
                ovrResult result = vrapi_SetDisplayRefreshRate(appState.Ovr, selectedFramerate);
            }

			// Get the HMD pose, predicted for the middle of the time period during which
			// the new eye images will be displayed. The number of frames predicted ahead
			// depends on the pipeline depth of the engine and the synthesis rate.
			// The better the prediction, the less black will be pulled in at the edges.
			const double predictedDisplayTime = vrapi_GetPredictedDisplayTime(appState.Ovr,
																			  appState.FrameIndex);
			const ovrTracking2 tracking = vrapi_GetPredictedTracking2(appState.Ovr,
																	  predictedDisplayTime);

			appState.DisplayTime = predictedDisplayTime;

            ovrApp_HandleInput(&appState);

			weaponHaptics();

			ovrSubmitFrameDescription2 frameDesc = { 0 };
			if (!useScreenLayer()) {

                ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
                layer.HeadPose = tracking.HeadPose;
                for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
                {
                    ovrFramebuffer * frameBuffer = &appState.Renderer.FrameBuffer[appState.Renderer.NumBuffers == 1 ? 0 : eye];
                    layer.Textures[eye].ColorSwapChain = frameBuffer->ColorTextureSwapChain;
                    layer.Textures[eye].SwapChainIndex = frameBuffer->TextureSwapChainIndex;

                    ovrMatrix4f projectionMatrix;
                    float fov = getFOV();
                    projectionMatrix = ovrMatrix4f_CreateProjectionFov(fov, fov,
                                                                       0.0f, 0.0f, 0.1f, 0.0f);

                    layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&projectionMatrix);

                    layer.Textures[eye].TextureRect.x = 0;
                    layer.Textures[eye].TextureRect.y = 0;
                    layer.Textures[eye].TextureRect.width = 1.0f;
                    layer.Textures[eye].TextureRect.height = 1.0f;
                }
                layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

                //Call the game drawing code to populate the cylinder layer texture
                ovrRenderer_RenderFrame(&appState.Renderer,
                            &appState.Java,
                            &tracking,
                            appState.Ovr);

                // Set up the description for this frame.
                const ovrLayerHeader2 *layers[] =
                        {
                                &layer.Header
                        };

                ovrSubmitFrameDescription2 frameDesc = {};
                frameDesc.Flags = 0;
                frameDesc.SwapInterval = appState.SwapInterval;
                frameDesc.FrameIndex = appState.FrameIndex;
                frameDesc.DisplayTime = appState.DisplayTime;
                frameDesc.LayerCount = 1;
                frameDesc.Layers = layers;

                // Hand over the eye images to the time warp.
                vrapi_SubmitFrame2(appState.Ovr, &frameDesc);

			} else {
				// Set-up the compositor layers for this frame.
				// NOTE: Multiple independent layers are allowed, but they need to be added
				// in a depth consistent order.
				memset( appState.Layers, 0, sizeof( ovrLayer_Union2 ) * ovrMaxLayerCount );
				appState.LayerCount = 0;

				// Add a simple cylindrical layer
				appState.Layers[appState.LayerCount++].Cylinder =
						BuildCylinderLayer( &appState.Scene.CylinderRenderer,
											appState.Scene.CylinderWidth, appState.Scene.CylinderHeight, &tracking, radians(playerYaw) );

				//Call the game drawing code to populate the cylinder layer texture
                ovrRenderer_RenderFrame(&appState.Scene.CylinderRenderer,
										&appState.Java,
										&tracking,
										appState.Ovr);


				// Compose the layers for this frame.
				const ovrLayerHeader2 * layerHeaders[ovrMaxLayerCount] = { 0 };
				for ( int i = 0; i < appState.LayerCount; i++ )
				{
					layerHeaders[i] = &appState.Layers[i].Header;
				}

				// Set up the description for this frame.
				frameDesc.Flags = 0;
				frameDesc.SwapInterval = appState.SwapInterval;
				frameDesc.FrameIndex = appState.FrameIndex;
				frameDesc.DisplayTime = appState.DisplayTime;
				frameDesc.LayerCount = appState.LayerCount;
				frameDesc.Layers = layerHeaders;

                // Hand over the eye images to the time warp.
                vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
            }
        }
        else
		{
		    //We are now shutting down
		    if (runStatus == 0)
            {
                //Give us half a second (36 frames)
                shutdownCountdown = 36;
                runStatus++;
            } else	if (runStatus == 1)
            {
                if (--shutdownCountdown == 0) {
                    runStatus++;
                }
            } else	if (runStatus == 2)
            {
                Host_Shutdown();
                runStatus++;
            } else if (runStatus == 3)
            {
                ovrRenderer_Destroy( &appState.Renderer );
                ovrEgl_DestroyContext( &appState.Egl );
                (*java.Vm)->DetachCurrentThread( java.Vm );
                vrapi_Shutdown();
                exit( 0 );
            }
		}
	}

	return NULL;
}

static void ovrAppThread_Create( ovrAppThread * appThread, JNIEnv * env, jobject activityObject, jclass activityClass )
{
	(*env)->GetJavaVM( env, &appThread->JavaVm );
	appThread->ActivityObject = (*env)->NewGlobalRef( env, activityObject );
	appThread->ActivityClass = (*env)->NewGlobalRef( env, activityClass );
	appThread->Thread = 0;
	appThread->NativeWindow = NULL;
	ovrMessageQueue_Create( &appThread->MessageQueue );

	const int createErr = pthread_create( &appThread->Thread, NULL, AppThreadFunction, appThread );
	if ( createErr != 0 )
	{
		ALOGE( "pthread_create returned %i", createErr );
	}
}

static void ovrAppThread_Destroy( ovrAppThread * appThread, JNIEnv * env )
{
	pthread_join( appThread->Thread, NULL );
	(*env)->DeleteGlobalRef( env, appThread->ActivityObject );
	(*env)->DeleteGlobalRef( env, appThread->ActivityClass );
	ovrMessageQueue_Destroy( &appThread->MessageQueue );
}

/*
================================================================================

Activity lifecycle

================================================================================
*/

int JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv *env;
	jVM = vm;
	if((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
	{
		ALOGE("Failed JNI_OnLoad");
		return -1;
	}

	return JNI_VERSION_1_4;
}

JNIEXPORT jlong JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onCreate( JNIEnv * env, jclass activityClass, jobject activity,
																	   jstring commandLineParams)
{
	ALOGV( "    GLES3JNILib::onCreate()" );

	/* the global arg_xxx structs are initialised within the argtable */
	void *argtable[] = {
			ss   = arg_dbl0("s", "supersampling", "<double>", "super sampling value (e.g. 1.0)"),
            cpu   = arg_int0("c", "cpu", "<int>", "CPU perf index 1-3 (default: 2)"),
            gpu   = arg_int0("g", "gpu", "<int>", "GPU perf index 1-3 (default: 3)"),
            refresh = arg_int0("r", "refresh", "<int>", "Refresh Rate (default: Quest 1: 72, Quest 2: 90)"),
            msaa   = arg_dbl0("m", "msaa", "<int>", "msaa value 1-4 (default 1)"), // Don't think this actually works
			end     = arg_end(20)
	};

	jboolean iscopy;
	const char *arg = (*env)->GetStringUTFChars(env, commandLineParams, &iscopy);

	char *cmdLine = NULL;
	if (arg && strlen(arg))
	{
		cmdLine = strdup(arg);
	}

	(*env)->ReleaseStringUTFChars(env, commandLineParams, arg);

	ALOGV("Command line %s", cmdLine);
	argv = malloc(sizeof(char*) * 255);
	argc = ParseCommandLine(strdup(cmdLine), argv);

	/* verify the argtable[] entries were allocated sucessfully */
	if (arg_nullcheck(argtable) == 0) {
		/* Parse the command line as defined by argtable[] */
		arg_parse(argc, argv, argtable);

        if (ss->count > 0 && ss->dval[0] > 0.0)
        {
            SS_MULTIPLIER = ss->dval[0];

            if (SS_MULTIPLIER > 1.2F)
            {
                SS_MULTIPLIER = 1.2F;
            }
        }

        if (cpu->count > 0 && cpu->ival[0] > 0 && cpu->ival[0] < 10)
        {
            CPU_LEVEL = cpu->ival[0];
        }

        if (gpu->count > 0 && gpu->ival[0] > 0 && gpu->ival[0] < 10)
        {
            GPU_LEVEL = gpu->ival[0];
        }

        if (refresh->count > 0 && refresh->ival[0] > 0 && refresh->ival[0] <= 120)
        {
            REFRESH = refresh->ival[0];
        }

        if (msaa->count > 0 && msaa->dval[0] > 0 && msaa->dval[0] < 5)
        {
            NUM_MULTI_SAMPLES = msaa->dval[0];
        }
	}

	ovrAppThread * appThread = (ovrAppThread *) malloc( sizeof( ovrAppThread ) );
	ovrAppThread_Create( appThread, env, activity, activityClass );

	ovrMessageQueue_Enable( &appThread->MessageQueue, true );
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_CREATE, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );

	return (jlong)((size_t)appThread);
}


JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_setCallbackObjects(JNIEnv *env, jobject obj, jobject obj2)
{
    (*jVM)->GetEnv(jVM, (void**) &env, JNI_VERSION_1_4);

	jclass qquestCallbackClass;

	qquestCallbackObj = (jobject)(*env)->NewGlobalRef(env, obj2);
	qquestCallbackClass = (*env)->GetObjectClass(env, qquestCallbackObj);
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onStart( JNIEnv * env, jobject obj, jlong handle)
{
	ALOGV( "    GLES3JNILib::onStart()" );

	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_START, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onResume( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onResume()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_RESUME, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onPause( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onPause()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_PAUSE, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onStop( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onStop()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_STOP, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onDestroy( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onDestroy()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_DESTROY, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
	ovrMessageQueue_Enable( &appThread->MessageQueue, false );

	ovrAppThread_Destroy( appThread, env );
	free( appThread );
}

/*
================================================================================

Surface lifecycle

================================================================================
*/

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onSurfaceCreated( JNIEnv * env, jobject obj, jlong handle, jobject surface )
{
	ALOGV( "    GLES3JNILib::onSurfaceCreated()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);

	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( env, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		ALOGE( "        Surface not in landscape mode!" );
	}

	ALOGV( "        NativeWindow = ANativeWindow_fromSurface( env, surface )" );
	appThread->NativeWindow = newNativeWindow;
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED );
	ovrMessage_SetPointerParm( &message, 0, appThread->NativeWindow );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onSurfaceChanged( JNIEnv * env, jobject obj, jlong handle, jobject surface )
{
	ALOGV( "    GLES3JNILib::onSurfaceChanged()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);

	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( env, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		ALOGE( "        Surface not in landscape mode!" );
	}

	if ( newNativeWindow != appThread->NativeWindow )
	{
		if ( appThread->NativeWindow != NULL )
		{
			ovrMessage message;
			ovrMessage_Init( &message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED );
			ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
			ALOGV( "        ANativeWindow_release( NativeWindow )" );
			ANativeWindow_release( appThread->NativeWindow );
			appThread->NativeWindow = NULL;
		}
		if ( newNativeWindow != NULL )
		{
			ALOGV( "        NativeWindow = ANativeWindow_fromSurface( env, surface )" );
			appThread->NativeWindow = newNativeWindow;
			ovrMessage message;
			ovrMessage_Init( &message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED );
			ovrMessage_SetPointerParm( &message, 0, appThread->NativeWindow );
			ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
		}
	}
	else if ( newNativeWindow != NULL )
	{
		ANativeWindow_release( newNativeWindow );
	}
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onSurfaceDestroyed( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onSurfaceDestroyed()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
	ALOGV( "        ANativeWindow_release( NativeWindow )" );
	ANativeWindow_release( appThread->NativeWindow );
	appThread->NativeWindow = NULL;
}

