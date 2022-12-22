#if !defined(vrcommon_h)
#define vrcommon_h

#include <android/log.h>

#include "../darkplaces/mathlib.h"

#include "TBXR_Common.h"

#define LOG_TAG "QuakeQuest"

#ifndef NDEBUG
#define DEBUG 1
#endif

#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

#if DEBUG
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#else
#define ALOGV(...)
#endif

extern ovrInputStateTrackedRemote leftTrackedRemoteState_old;
extern ovrInputStateTrackedRemote leftTrackedRemoteState_new;
extern ovrTrackedController leftRemoteTracking_new;
extern ovrInputStateTrackedRemote rightTrackedRemoteState_old;
extern ovrInputStateTrackedRemote rightTrackedRemoteState_new;
extern ovrTrackedController rightRemoteTracking_new;

extern float playerHeight;
extern float playerYaw;

extern vec3_t hmdorientation;

qboolean isMultiplayer();
float length(float x, float y);
float nonLinearFilter(float in);
bool between(float min, float val, float max);

#endif //vrcommon_h