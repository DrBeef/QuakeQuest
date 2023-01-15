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
#include "VrCommon.h"


#include "../darkplaces/qtypes.h"
#include "../darkplaces/quakedef.h"
#include "../darkplaces/menu.h"

//#define ENABLE_GL_DEBUG
#define ENABLE_GL_DEBUG_VERBOSE 1

//Let's go to the maximum!
extern int NUM_MULTI_SAMPLES;
extern int REFRESH	         ;
extern float SS_MULTIPLIER    ;


/* global arg_xxx structs */
struct arg_dbl *ss;
struct arg_int *cpu;
struct arg_int *gpu;
struct arg_int *msaa;
struct arg_int *refresh;
struct arg_end *end;

char **argv;
int argc=0;

float playerHeight;
float playerYaw;

extern float worldPosition[3];
float hmdPosition[3];
float playerHeight;
float positionDeltaThisFrame[3];
vec3_t hmdorientation;
extern float gunangles[3];
float weaponOffset[3];
float weaponVelocity[3];
qboolean weapon_stabilised;

int bigScreen = 1;
extern client_static_t	cls;

extern int stereoMode;

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

ovrInputStateTrackedRemote leftTrackedRemoteState_old;
ovrInputStateTrackedRemote leftTrackedRemoteState_new;
ovrTrackedController leftRemoteTracking_new;
ovrInputStateTrackedRemote rightTrackedRemoteState_old;
ovrInputStateTrackedRemote rightTrackedRemoteState_new;
ovrTrackedController rightRemoteTracking_new;

void QC_BeginFrame();
void QC_DrawFrame(int eye, int x, int y);
void QC_EndFrame();
void QC_KeyEvent(int state,int key,int character);
void QC_MoveEvent(float yaw, float pitch, float roll);
void QC_SetResolution(int width, int height);
void QC_Analog(int enable,float x,float y);
void QC_MotionEvent(float delta, float dx, float dy);
int main (int argc, char **argv);
extern	int			key_consoleactive;

static bool quake_initialised = false;

/*
================================================================================

QuakeQuest Stuff

================================================================================
*/

bool VR_UseScreenLayer()
{
	return (bigScreen != 0 || cls.demoplayback || key_consoleactive);
}

float VR_GetScreenLayerDistance()
{
	return (4.5f);
}

void BigScreenMode(int mode)
{
	if (bigScreen != 2)
	{
		bigScreen = mode;
	}
}

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


void VR_SetHMDOrientation(float pitch, float yaw, float roll)
{
	VectorSet(hmdorientation, pitch, yaw, roll);

	if (!VR_UseScreenLayer())
    {
    	playerYaw = yaw;
	}
}

void VR_SetHMDPosition(float x, float y, float z )
{
    positionDeltaThisFrame[0] = (worldPosition[0] - x);
    positionDeltaThisFrame[1] = (worldPosition[1] - y);
    positionDeltaThisFrame[2] = (worldPosition[2] - z);

    worldPosition[0] = x;
    worldPosition[1] = y;
    worldPosition[2] = z;
	
	static bool s_useScreen = false;

	VectorSet(hmdPosition, x, y, z);

    if (s_useScreen != VR_UseScreenLayer())
    {
		s_useScreen = VR_UseScreenLayer();

		//Record player height on transition
        playerHeight = y;
    }
}


void VR_Init()
{
	//init randomiser
	srand(time(NULL));
	
	chdir("/sdcard/QuakeQuest");
}

extern int runStatus;
void QC_exit(int exitCode)
{
	runStatus = exitCode;
}
void * AppThreadFunction(void * parm ) {
	gAppThread = (ovrAppThread *) parm;

	java.Vm = gAppThread->JavaVm;
	(*java.Vm)->AttachCurrentThread( java.Vm, &java.Env, NULL );
	java.ActivityObject = gAppThread->ActivityObject;

	// Note that AttachCurrentThread will reset the thread name.
	prctl(PR_SET_NAME, (long) "AppThreadFunction", 0, 0, 0);

	gAppState.MainThreadTid = gettid();
	

	TBXR_InitialiseOpenXR();

	VR_Init();

	TBXR_EnterVR();
	TBXR_InitRenderer();
	TBXR_InitActions();
	TBXR_WaitForSessionActive();

	//start
	{
		ALOGV( "    Initialising Quake Engine" );

		QC_SetResolution((int)gAppState.Width, (int)gAppState.Height);

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
	
	while (runStatus == -1)
	{
		TBXR_FrameSetup();
		
		//Set move information - if showing menu, don't pass head orientation through
		if (m_state == m_none)
			QC_MoveEvent(hmdorientation[YAW], hmdorientation[PITCH], hmdorientation[ROLL]);
		else
			QC_MoveEvent(0, 0, 0);

		//Set everything up
		QC_BeginFrame(/* true to stop time if needed in future */ false);

		// Render the eye images.
		for ( int eye = 0; eye < ovrMaxNumEyes; eye++ )
		{
			TBXR_prepareEyeBuffer(eye);

			//Now do the drawing for this eye
			QC_DrawFrame(eye, 0, 0);

			TBXR_finishEyeBuffer(eye);
		}

		QC_EndFrame();

		//Now trigger any haptics for this frame - QuakeQuest does this a bit differently
		//so we don't bother to provide any params
		VR_HapticEvent(NULL, 0, 0, 0, 0, 0);
		
		TBXR_submitFrame();
	}

	{
		TBXR_LeaveVR();
		//Ask Java to shut down
		VR_Shutdown();

		exit(0); // in case Java doesn't do the job
	}

	return NULL;
}

//All the stuff we want to do each frame specifically for this game
void VR_FrameSetup()
{

}

bool VR_GetVRProjection(int eye, float zNear, float zFar, float* projection)
{
#ifdef PICO_XR
	XrMatrix4x4f_CreateProjectionFov(
			&(gAppState.ProjectionMatrices[eye]), GRAPHICS_OPENGL_ES,
			gAppState.Projections[eye].fov, zNear, zFar);
#endif

#ifdef META_QUEST
	XrFovf fov = {};
	for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
		fov.angleLeft += gAppState.Projections[eye].fov.angleLeft / 2.0f;
		fov.angleRight += gAppState.Projections[eye].fov.angleRight / 2.0f;
		fov.angleUp += gAppState.Projections[eye].fov.angleUp / 2.0f;
		fov.angleDown += gAppState.Projections[eye].fov.angleDown / 2.0f;
	}
	XrMatrix4x4f_CreateProjectionFov(
			&(gAppState.ProjectionMatrices[eye]), GRAPHICS_OPENGL_ES,
			fov, zNear, zFar);
#endif

	memcpy(projection, gAppState.ProjectionMatrices[eye].m, 16 * sizeof(float));
	return true;
}


/*
 *  event - name of event
 *  position - for the use of external haptics providers to indicate which bit of haptic hardware should be triggered
 *  flags - a way for the code to specify which controller to produce haptics on, if 0 then weaponFireChannel is calculated in this function
 *  intensity - 0-100
 *  angle - yaw angle (again for external haptics devices) to place the feedback correctly
 *  yHeight - for external haptics devices to place the feedback correctly
 */
void VR_HapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight )
{
	if (VR_UseScreenLayer())
	{
		return;
	}

	qboolean isFirePressed = false;
	if (cl_righthanded.integer) {
		//Fire
		isFirePressed = rightTrackedRemoteState_new.Buttons & xrButton_Trigger;
	} else {
		isFirePressed = leftTrackedRemoteState_new.Buttons & xrButton_Trigger;
	}

	if (isFirePressed)
	{
		static double timeLastHaptic = 0;
		double timeNow = TBXR_GetTimeInMilliSeconds();

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
            int channel = weapon_stabilised ? 3 : (cl_righthanded.integer ? 2 : 1);
			TBXR_Vibrate(hapticLength, channel, hapticLevel);
		}
	}
}


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

float GetSysTicrate()
{
	return 1.0F / (float)(TBXR_GetRefresh());
}

float length(float x, float y)
{
	return sqrtf(powf(x, 2.0f) + powf(y, 2.0f));
}

//Timing stuff for joypad control
static long oldtime=0;
long delta=0;

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

static void HandleInput_Default(  )
{
    float remote_movementSideways = 0.0f;
    float remote_movementForward = 0.0f;
    float positional_movementSideways = 0.0f;
    float positional_movementForward = 0.0f;
    float controllerAngles[3];

    //The amount of yaw changed by controller
    float yawOffset = cl.viewangles[YAW] - hmdorientation[YAW];

    ovrInputStateTrackedRemote *dominantTrackedRemoteState = cl_righthanded.integer ? &rightTrackedRemoteState_new : &leftTrackedRemoteState_new;
    ovrInputStateTrackedRemote *dominantTrackedRemoteStateOld = cl_righthanded.integer ? &rightTrackedRemoteState_old : &leftTrackedRemoteState_old;
	ovrTrackedController *dominantRemoteTracking = cl_righthanded.integer ? &rightRemoteTracking_new : &leftRemoteTracking_new;
	ovrInputStateTrackedRemote *offHandTrackedRemoteState = !cl_righthanded.integer ? &rightTrackedRemoteState_new : &leftTrackedRemoteState_new;
	ovrInputStateTrackedRemote *offHandTrackedRemoteStateOld = !cl_righthanded.integer ? &rightTrackedRemoteState_old : &leftTrackedRemoteState_old;
	ovrTrackedController *offHandRemoteTracking = !cl_righthanded.integer ? &rightRemoteTracking_new : &leftRemoteTracking_new;

	if (textInput)
    {
        //Toggle text input
        if ((leftTrackedRemoteState_new.Buttons & xrButton_Y) &&
            (leftTrackedRemoteState_new.Buttons & xrButton_Y) !=
            (leftTrackedRemoteState_old.Buttons & xrButton_Y)) {
            textInput = !textInput;
            SCR_CenterPrint("Text Input: Disabled");
        }

        int left_char_index = getCharacter(leftTrackedRemoteState_new.Joystick.x, leftTrackedRemoteState_new.Joystick.y);
        int right_char_index = getCharacter(rightTrackedRemoteState_new.Joystick.x, rightTrackedRemoteState_new.Joystick.y);

        //Toggle Shift
        if ((leftTrackedRemoteState_new.Buttons & xrButton_X) &&
            (leftTrackedRemoteState_new.Buttons & xrButton_X) !=
            (leftTrackedRemoteState_old.Buttons & xrButton_X)) {
            shift = 1 - shift;
        }

        //Cycle Left Grid
        if ((leftTrackedRemoteState_new.Buttons & xrButton_GripTrigger) &&
            (leftTrackedRemoteState_new.Buttons & xrButton_GripTrigger) !=
            (leftTrackedRemoteState_old.Buttons & xrButton_GripTrigger)) {
            left_grid = (++left_grid) % 3;
        }

        //Cycle Right Grid
        if ((rightTrackedRemoteState_new.Buttons & xrButton_GripTrigger) &&
            (rightTrackedRemoteState_new.Buttons & xrButton_GripTrigger) !=
            (rightTrackedRemoteState_old.Buttons & xrButton_GripTrigger)) {
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
        if ((rightTrackedRemoteState_new.Buttons & xrButton_A) !=
            (rightTrackedRemoteState_old.Buttons & xrButton_A)) {
            QC_KeyEvent((rightTrackedRemoteState_new.Buttons & xrButton_A) > 0 ? 1 : 0, K_ENTER, 0);
        }

        //Delete
        if ((rightTrackedRemoteState_new.Buttons & xrButton_B) !=
            (rightTrackedRemoteState_old.Buttons & xrButton_B)) {
            QC_KeyEvent((rightTrackedRemoteState_new.Buttons & xrButton_B) > 0 ? 1 : 0, K_BACKSPACE, 0);
        }

        //Use Left Character
        if ((leftTrackedRemoteState_new.Buttons & xrButton_Trigger) !=
            (leftTrackedRemoteState_old.Buttons & xrButton_Trigger)) {
            QC_KeyEvent((leftTrackedRemoteState_new.Buttons & xrButton_Trigger) > 0 ? 1 : 0,
                        left_char, left_char);
        }

        //Use Right Character
        if ((rightTrackedRemoteState_new.Buttons & xrButton_Trigger) !=
            (rightTrackedRemoteState_old.Buttons & xrButton_Trigger)) {
            QC_KeyEvent((rightTrackedRemoteState_new.Buttons & xrButton_Trigger) > 0 ? 1 : 0,
                        right_char, right_char);
        }

        //Menu button - could be on left or right controller
        handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old,
                                      xrButton_Enter, K_ESCAPE);
		handleTrackedControllerButton(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old,
									  xrButton_Enter, K_ESCAPE);


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
		float distance = sqrtf(powf(offHandRemoteTracking->Pose.position.x - dominantRemoteTracking->Pose.position.x, 2) +
							   powf(offHandRemoteTracking->Pose.position.y - dominantRemoteTracking->Pose.position.y, 2) +
							   powf(offHandRemoteTracking->Pose.position.z - dominantRemoteTracking->Pose.position.z, 2));

        //dominant hand stuff first
        weapon_stabilised = distance < 0.5f &&
                (offHandTrackedRemoteState->Buttons & xrButton_GripTrigger) &&
                cl.stats[STAT_ACTIVEWEAPON] != IT_AXE;

        {
            weaponOffset[0] = dominantRemoteTracking->Pose.position.x - hmdPosition[0];
            weaponOffset[1] = dominantRemoteTracking->Pose.position.y - hmdPosition[1];
            weaponOffset[2] = dominantRemoteTracking->Pose.position.z - hmdPosition[2];

            weaponVelocity[0] = dominantRemoteTracking->Velocity.linearVelocity.x;
            weaponVelocity[1] = dominantRemoteTracking->Velocity.linearVelocity.y;
            weaponVelocity[2] = dominantRemoteTracking->Velocity.linearVelocity.z;

            ///Weapon location relative to view
            vec2_t v;
            rotateAboutOrigin(weaponOffset[0], weaponOffset[2], -yawOffset, v);
            weaponOffset[0] = v[0];
            weaponOffset[2] = v[1];

            //Set gun angles
            const XrQuaternionf quatRemote = dominantRemoteTracking->Pose.orientation;
			vec3_t rotation = {vr_weaponpitchadjust.value, 0, 0};
			QuatToYawPitchRoll(quatRemote, rotation, gunangles);

            if (weapon_stabilised)
            {
                float z = offHandRemoteTracking->Pose.position.z - dominantRemoteTracking->Pose.position.z;
                float x = offHandRemoteTracking->Pose.position.x - dominantRemoteTracking->Pose.position.x;
                float y = offHandRemoteTracking->Pose.position.y - dominantRemoteTracking->Pose.position.y;
                float zxDist = length(x, z);

                if (zxDist != 0.0f && z != 0.0f) {
                    VectorSet(gunangles, -RAD2DEG(atanf(y / zxDist)),  -RAD2DEG(atan2f(x, -z)), gunangles[ROLL]);
                }
            }

            gunangles[YAW] += yawOffset;

            //Change laser sight on joystick click
            if ((dominantTrackedRemoteState->Buttons & xrButton_Joystick) &&
                (dominantTrackedRemoteState->Buttons & xrButton_Joystick) !=
                (dominantTrackedRemoteStateOld->Buttons & xrButton_Joystick)) {
                Cvar_SetValueQuick(&r_lasersight, (r_lasersight.integer + 1) % 3);
            }
        }

        //off-hand stuff
        float controllerYawHeading;
        float hmdYawHeading;
        {
			vec3_t rotation = {0, 0, 0};
            QuatToYawPitchRoll(offHandRemoteTracking->Pose.orientation, rotation,
                               controllerAngles);

            controllerYawHeading = controllerAngles[YAW] - gunangles[YAW] + yawOffset;
            hmdYawHeading = hmdorientation[YAW] - gunangles[YAW] + yawOffset;
        }

        //Right-hand specific stuff
        {
            ALOGE("        Right-Controller-Position: %f, %f, %f",
                  rightRemoteTracking_new.Pose.position.x,
                  rightRemoteTracking_new.Pose.position.y,
                  rightRemoteTracking_new.Pose.position.z);

            //This section corrects for the fact that the controller actually controls direction of movement, but we want to move relative to the direction the
            //player is facing for positional tracking
            float multiplier = (float)(2300.0f * (TBXR_GetRefresh() / 72.0) ) /
                               (cl_movementspeed.value * ((offHandTrackedRemoteState->Buttons & xrButton_Trigger) ? cl_movespeedkey.value : 1.0f));

            vec2_t v;
            rotateAboutOrigin(-positionDeltaThisFrame[0] * multiplier,
                              positionDeltaThisFrame[2] * multiplier, yawOffset - gunangles[YAW], v);
            positional_movementSideways = v[0];
            positional_movementForward = v[1];


            long t = (long)TBXR_GetTimeInMilliSeconds();
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
                                              &rightTrackedRemoteState_old, xrButton_A, K_ENTER);

                //Back button
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old, xrButton_B, K_ESCAPE);
            } else {
                //Jump
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old, xrButton_A, K_SPACE);

				//Adjust weapon aim pitch
				if ((rightTrackedRemoteState_new.Buttons & xrButton_B) &&
					(rightTrackedRemoteState_new.Buttons & xrButton_B) !=
					(rightTrackedRemoteState_old.Buttons & xrButton_B)) {

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
                                              xrButton_Trigger, K_MOUSE1);
            } else {
                //Run
                handleTrackedControllerButton(&rightTrackedRemoteState_new,
                                              &rightTrackedRemoteState_old,
                                              xrButton_Trigger, K_SHIFT);
            }

            rightTrackedRemoteState_old = rightTrackedRemoteState_new;

        }

        //Left-hand specific stuff
        {
            ALOGE("        Left-Controller-Position: %f, %f, %f",
                  leftRemoteTracking_new.Pose.position.x,
                  leftRemoteTracking_new.Pose.position.y,
                  leftRemoteTracking_new.Pose.position.z);

            //Menu button
            handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old,
                                          xrButton_Enter, K_ESCAPE);

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
                                              xrButton_Trigger, K_SHIFT);
            } else {
                //Fire
                handleTrackedControllerButton(&leftTrackedRemoteState_new,
                                              &leftTrackedRemoteState_old,
                                              xrButton_Trigger, K_MOUSE1);
            }

            static bool canUseQuickSave = false;
            if (canUseQuickSave)
            {
                if ((leftTrackedRemoteState_new.Buttons & xrButton_X) &&
                    (leftTrackedRemoteState_new.Buttons & xrButton_X) !=
                    (leftTrackedRemoteState_old.Buttons & xrButton_X)) {
                    Cbuf_InsertText("save quick\n");

                    //Vibrate to let user know they successfully saved
					SCR_CenterPrint("Quick Saved");
                    TBXR_Vibrate(500, cl_righthanded.integer ? 1 : 2, 1.0);
                }

                if ((leftTrackedRemoteState_new.Buttons & xrButton_Y) &&
                    (leftTrackedRemoteState_new.Buttons & xrButton_Y) !=
                    (leftTrackedRemoteState_old.Buttons & xrButton_Y)) {
                    Cbuf_InsertText("load quick");
                }
            }
            else {
#ifndef NDEBUG
                //Give all weapons and all ammo and god mode
                if ((leftTrackedRemoteState_new.Buttons & xrButton_X) &&
                    (leftTrackedRemoteState_new.Buttons & xrButton_X) !=
                    (leftTrackedRemoteState_old.Buttons & xrButton_X)) {
                    Cbuf_InsertText("God\n");
                    Cbuf_InsertText("Impulse 9\n");
                    breakHere = 1;
                }
#endif

                //Toggle text input
                if ((leftTrackedRemoteState_new.Buttons & xrButton_Y) &&
                    (leftTrackedRemoteState_new.Buttons & xrButton_Y) !=
                    (leftTrackedRemoteState_old.Buttons & xrButton_Y)) {
                    textInput = !textInput;
                }
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

void VR_HandleControllerInput() {
	TBXR_UpdateControllers();

	HandleInput_Default();
}

/*
================================================================================

Activity lifecycle

================================================================================
*/

jmethodID android_shutdown;
static JavaVM *jVM;
static jobject jniCallbackObj=0;

void jni_shutdown()
{
	ALOGV("Calling: jni_shutdown");
	JNIEnv *env;
	jobject tmp;
	if (((*jVM)->GetEnv(jVM, (void**) &env, JNI_VERSION_1_4))<0)
	{
		(*jVM)->AttachCurrentThread( jVM, &env, NULL );
	}
	return (*env)->CallVoidMethod(env, jniCallbackObj, android_shutdown);
}

void VR_Shutdown()
{
	jni_shutdown();
}


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
			ss    = arg_dbl0("s", "supersampling", "<double>", "super sampling value (default: Q1: 1.2, Q2: 1.35)"),
            cpu   = arg_int0("c", "cpu", "<int>", "CPU perf index 1-4 (default: 2)"),
            gpu   = arg_int0("g", "gpu", "<int>", "GPU perf index 1-4 (default: 3)"),
            msaa  = arg_int0("m", "msaa", "<int>", "MSAA (default: 1)"),
            refresh  = arg_int0("r", "refresh", "<int>", "Refresh Rate (default: Q1: 72, Q2: 72)"),
            end   = arg_end(20)
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
	argv = (char**)malloc(sizeof(char*) * 255);
	argc = ParseCommandLine(strdup(cmdLine), argv);

	/* verify the argtable[] entries were allocated sucessfully */
	if (arg_nullcheck(argtable) == 0) {
		/* Parse the command line as defined by argtable[] */
		arg_parse(argc, argv, argtable);

        if (ss->count > 0 && ss->dval[0] > 0.0)
        {
            SS_MULTIPLIER = ss->dval[0];
        }

        if (msaa->count > 0 && msaa->ival[0] > 0 && msaa->ival[0] < 10)
        {
            NUM_MULTI_SAMPLES = msaa->ival[0];
        }

        if (refresh->count > 0 && refresh->ival[0] > 0 && refresh->ival[0] <= 120)
        {
            REFRESH = refresh->ival[0];
        }
	}

	ovrAppThread * appThread = (ovrAppThread *) malloc( sizeof( ovrAppThread ) );
	ovrAppThread_Create( appThread, env, activity, activityClass );

	surfaceMessageQueue_Enable(&appThread->MessageQueue, true);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_CREATE, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);

	return (jlong)((size_t)appThread);
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onStart( JNIEnv * env, jobject obj, jlong handle, jobject obj1)
{
	ALOGV( "    GLES3JNILib::onStart()" );

	jniCallbackObj = (jobject)((*env)->NewGlobalRef(env, obj1));
	jclass callbackClass = (*env)->GetObjectClass(env, jniCallbackObj);
	android_shutdown = (*env)->GetMethodID(env, callbackClass,"shutdown","()V");

	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	srufaceMessage message;
	surfaceMessage_Init( &message, MESSAGE_ON_START, MQ_WAIT_PROCESSED );
	surfaceMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onResume( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onResume()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_RESUME, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onPause( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onPause()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_PAUSE, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onStop( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onStop()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_STOP, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL Java_com_drbeef_quakequest_GLES3JNILib_onDestroy( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onDestroy()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_DESTROY, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
	surfaceMessageQueue_Enable(&appThread->MessageQueue, false);

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
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED);
	surfaceMessage_SetPointerParm(&message, 0, appThread->NativeWindow);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
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
			srufaceMessage message;
			surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
			surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
			ALOGV( "        ANativeWindow_release( NativeWindow )" );
			ANativeWindow_release( appThread->NativeWindow );
			appThread->NativeWindow = NULL;
		}
		if ( newNativeWindow != NULL )
		{
			ALOGV( "        NativeWindow = ANativeWindow_fromSurface( env, surface )" );
			appThread->NativeWindow = newNativeWindow;
			srufaceMessage message;
			surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED);
			surfaceMessage_SetPointerParm(&message, 0, appThread->NativeWindow);
			surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
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
	srufaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
	ALOGV( "        ANativeWindow_release( NativeWindow )" );
	ANativeWindow_release( appThread->NativeWindow );
	appThread->NativeWindow = NULL;
}


