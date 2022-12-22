#include "VrCommon.h"

#ifdef META_QUEST

extern ovrApp gAppState;

XrSpace CreateActionSpace(XrAction poseAction, XrPath subactionPath) {
    XrActionSpaceCreateInfo asci = {};
    asci.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
    asci.action = poseAction;
    asci.poseInActionSpace.orientation.w = 1.0f;
    asci.subactionPath = subactionPath;
    XrSpace actionSpace = XR_NULL_HANDLE;
    OXR(xrCreateActionSpace(gAppState.Session, &asci, &actionSpace));
    return actionSpace;
}

XrActionSuggestedBinding ActionSuggestedBinding(XrAction action, const char* bindingString) {
    XrActionSuggestedBinding asb;
    asb.action = action;
    XrPath bindingPath;
    OXR(xrStringToPath(gAppState.Instance, bindingString, &bindingPath));
    asb.binding = bindingPath;
    return asb;
}

XrActionSet CreateActionSet(int priority, const char* name, const char* localizedName) {
    XrActionSetCreateInfo asci = {};
    asci.type = XR_TYPE_ACTION_SET_CREATE_INFO;
    asci.next = NULL;
    asci.priority = priority;
    strcpy(asci.actionSetName, name);
    strcpy(asci.localizedActionSetName, localizedName);
    XrActionSet actionSet = XR_NULL_HANDLE;
    OXR(xrCreateActionSet(gAppState.Instance, &asci, &actionSet));
    return actionSet;
}

XrAction CreateAction(
        XrActionSet actionSet,
        XrActionType type,
        const char* actionName,
        const char* localizedName,
        int countSubactionPaths,
        XrPath* subactionPaths) {
    ALOGV("CreateAction %s, %" PRIi32, actionName, countSubactionPaths);

    XrActionCreateInfo aci = {};
    aci.type = XR_TYPE_ACTION_CREATE_INFO;
    aci.next = NULL;
    aci.actionType = type;
    if (countSubactionPaths > 0) {
        aci.countSubactionPaths = countSubactionPaths;
        aci.subactionPaths = subactionPaths;
    }
    strcpy(aci.actionName, actionName);
    strcpy(aci.localizedActionName, localizedName ? localizedName : actionName);
    XrAction action = XR_NULL_HANDLE;
    OXR(xrCreateAction(actionSet, &aci, &action));
    return action;
}

bool ActionPoseIsActive(XrAction action, XrPath subactionPath) {
    XrActionStateGetInfo getInfo = {};
    getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
    getInfo.action = action;
    getInfo.subactionPath = subactionPath;

    XrActionStatePose state = {};
    state.type = XR_TYPE_ACTION_STATE_POSE;
    OXR(xrGetActionStatePose(gAppState.Session, &getInfo, &state));
    return state.isActive != XR_FALSE;
}

XrActionStateFloat GetActionStateFloat(XrAction action) {
    XrActionStateGetInfo getInfo = {};
    getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
    getInfo.action = action;

    XrActionStateFloat state = {};
    state.type = XR_TYPE_ACTION_STATE_FLOAT;

    OXR(xrGetActionStateFloat(gAppState.Session, &getInfo, &state));
    return state;
}

XrActionStateBoolean GetActionStateBoolean(XrAction action) {
    XrActionStateGetInfo getInfo = {};
    getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
    getInfo.action = action;

    XrActionStateBoolean state = {};
    state.type = XR_TYPE_ACTION_STATE_BOOLEAN;

    OXR(xrGetActionStateBoolean(gAppState.Session, &getInfo, &state));
    return state;
}

XrActionStateVector2f GetActionStateVector2(XrAction action) {
    XrActionStateGetInfo getInfo = {};
    getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
    getInfo.action = action;

    XrActionStateVector2f state = {};
    state.type = XR_TYPE_ACTION_STATE_VECTOR2F;

    OXR(xrGetActionStateVector2f(gAppState.Session, &getInfo, &state));
    return state;
}


//OpenXR
XrPath leftHandPath;
XrPath rightHandPath;
XrAction handPoseLeftAction;
XrAction handPoseRightAction;
XrAction indexLeftAction;
XrAction indexRightAction;
XrAction menuAction;
XrAction buttonAAction;
XrAction buttonBAction;
XrAction buttonXAction;
XrAction buttonYAction;
XrAction gripLeftAction;
XrAction gripRightAction;
XrAction moveOnLeftJoystickAction;
XrAction moveOnRightJoystickAction;
XrAction thumbstickLeftClickAction;
XrAction thumbstickRightClickAction;
XrAction vibrateLeftFeedback;
XrAction vibrateRightFeedback;
XrActionSet runningActionSet;
XrSpace leftControllerAimSpace = XR_NULL_HANDLE;
XrSpace rightControllerAimSpace = XR_NULL_HANDLE;
bool inputInitialized = false;
bool useSimpleProfile = false;


//0 = left, 1 = right
float vibration_channel_duration[2] = {0.0f, 0.0f};
float vibration_channel_intensity[2] = {0.0f, 0.0f};

void TBXR_InitActions( void )
{
    // Actions
    runningActionSet = CreateActionSet(1, "running_action_set", "Action Set used on main loop");
    indexLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "index_left", "Index left", 0, NULL);
    indexRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "index_right", "Index right", 0, NULL);
    menuAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu_action", "Menu", 0, NULL);
    buttonAAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a", "Button A", 0, NULL);
    buttonBAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b", "Button B", 0, NULL);
    buttonXAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x", "Button X", 0, NULL);
    buttonYAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y", "Button Y", 0, NULL);
    gripLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "grip_left", "Grip left", 0, NULL);
    gripRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "grip_right", "Grip right", 0, NULL);
    moveOnLeftJoystickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "move_on_left_joy", "Move on left Joy", 0, NULL);
    moveOnRightJoystickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "move_on_right_joy", "Move on right Joy", 0, NULL);
    thumbstickLeftClickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_left", "Thumbstick left", 0, NULL);
    thumbstickRightClickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_right", "Thumbstick right", 0, NULL);
    vibrateLeftFeedback = CreateAction(runningActionSet, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_left_feedback", "Vibrate Left Controller Feedback", 0, NULL);
    vibrateRightFeedback = CreateAction(runningActionSet, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_right_feedback", "Vibrate Right Controller Feedback", 0, NULL);

    OXR(xrStringToPath(gAppState.Instance, "/user/hand/left", &leftHandPath));
    OXR(xrStringToPath(gAppState.Instance, "/user/hand/right", &rightHandPath));
    handPoseLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_POSE_INPUT, "hand_pose_left", NULL, 1, &leftHandPath);
    handPoseRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_POSE_INPUT, "hand_pose_right", NULL, 1, &rightHandPath);

    if (leftControllerAimSpace == XR_NULL_HANDLE) {
        leftControllerAimSpace = CreateActionSpace(handPoseLeftAction, leftHandPath);
    }
    if (rightControllerAimSpace == XR_NULL_HANDLE) {
        rightControllerAimSpace = CreateActionSpace(handPoseRightAction, rightHandPath);
    }

    XrPath interactionProfilePath = XR_NULL_PATH;
    XrPath interactionProfilePathTouch = XR_NULL_PATH;
    XrPath interactionProfilePathKHRSimple = XR_NULL_PATH;

    OXR(xrStringToPath(gAppState.Instance, "/interaction_profiles/oculus/touch_controller", &interactionProfilePathTouch));
    OXR(xrStringToPath(gAppState.Instance, "/interaction_profiles/khr/simple_controller", &interactionProfilePathKHRSimple));

    // Toggle this to force simple as a first choice, otherwise use it as a last resort
    if (useSimpleProfile) {
        ALOGV("xrSuggestInteractionProfileBindings found bindings for Khronos SIMPLE controller");
        interactionProfilePath = interactionProfilePathKHRSimple;
    } else {
        // Query Set
        XrActionSet queryActionSet = CreateActionSet(1, "query_action_set", "Action Set used to query device caps");
        XrAction dummyAction = CreateAction(queryActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "dummy_action", "Dummy Action", 0, NULL);

        // Map bindings
        XrActionSuggestedBinding bindings[1];
        int currBinding = 0;
        bindings[currBinding++] = ActionSuggestedBinding(dummyAction, "/user/hand/right/input/system/click");

        XrInteractionProfileSuggestedBinding suggestedBindings = {};
        suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        suggestedBindings.next = NULL;
        suggestedBindings.suggestedBindings = bindings;
        suggestedBindings.countSuggestedBindings = currBinding;

        // Try all
        suggestedBindings.interactionProfile = interactionProfilePathTouch;
        XrResult suggestTouchResult = xrSuggestInteractionProfileBindings(gAppState.Instance, &suggestedBindings);
        OXR(suggestTouchResult);

        if (XR_SUCCESS == suggestTouchResult) {
            ALOGV("xrSuggestInteractionProfileBindings found bindings for QUEST controller");
            interactionProfilePath = interactionProfilePathTouch;
        }

        if (interactionProfilePath == XR_NULL_PATH) {
            // Simple as a fallback
            bindings[0] = ActionSuggestedBinding(dummyAction, "/user/hand/right/input/select/click");
            suggestedBindings.interactionProfile = interactionProfilePathKHRSimple;
            XrResult suggestKHRSimpleResult = xrSuggestInteractionProfileBindings(gAppState.Instance, &suggestedBindings);
            OXR(suggestKHRSimpleResult);
            if (XR_SUCCESS == suggestKHRSimpleResult) {
                ALOGV("xrSuggestInteractionProfileBindings found bindings for Khronos SIMPLE controller");
                interactionProfilePath = interactionProfilePathKHRSimple;
            } else {
                ALOGE("xrSuggestInteractionProfileBindings did NOT find any bindings.");
                assert(false);
            }
        }
    }

    // Action creation
    {
        // Map bindings
        XrActionSuggestedBinding bindings[32]; // large enough for all profiles
        int currBinding = 0;

        {
            if (interactionProfilePath == interactionProfilePathTouch) {
                bindings[currBinding++] = ActionSuggestedBinding(indexLeftAction, "/user/hand/left/input/trigger");
                bindings[currBinding++] = ActionSuggestedBinding(indexRightAction, "/user/hand/right/input/trigger");
                bindings[currBinding++] = ActionSuggestedBinding(menuAction, "/user/hand/left/input/menu/click");
                bindings[currBinding++] = ActionSuggestedBinding(buttonXAction, "/user/hand/left/input/x/click");
                bindings[currBinding++] = ActionSuggestedBinding(buttonYAction, "/user/hand/left/input/y/click");
                bindings[currBinding++] = ActionSuggestedBinding(buttonAAction, "/user/hand/right/input/a/click");
                bindings[currBinding++] = ActionSuggestedBinding(buttonBAction, "/user/hand/right/input/b/click");
                bindings[currBinding++] = ActionSuggestedBinding(gripLeftAction, "/user/hand/left/input/squeeze/value");
                bindings[currBinding++] = ActionSuggestedBinding(gripRightAction, "/user/hand/right/input/squeeze/value");
                bindings[currBinding++] = ActionSuggestedBinding(moveOnLeftJoystickAction, "/user/hand/left/input/thumbstick");
                bindings[currBinding++] = ActionSuggestedBinding(moveOnRightJoystickAction, "/user/hand/right/input/thumbstick");
                bindings[currBinding++] = ActionSuggestedBinding(thumbstickLeftClickAction, "/user/hand/left/input/thumbstick/click");
                bindings[currBinding++] = ActionSuggestedBinding(thumbstickRightClickAction, "/user/hand/right/input/thumbstick/click");
                bindings[currBinding++] = ActionSuggestedBinding(vibrateLeftFeedback, "/user/hand/left/output/haptic");
                bindings[currBinding++] = ActionSuggestedBinding(vibrateRightFeedback, "/user/hand/right/output/haptic");
                bindings[currBinding++] = ActionSuggestedBinding(handPoseLeftAction, "/user/hand/left/input/aim/pose");
                bindings[currBinding++] = ActionSuggestedBinding(handPoseRightAction, "/user/hand/right/input/aim/pose");
            }

            if (interactionProfilePath == interactionProfilePathKHRSimple) {
                bindings[currBinding++] = ActionSuggestedBinding(indexLeftAction, "/user/hand/left/input/select/click");
                bindings[currBinding++] = ActionSuggestedBinding(indexRightAction, "/user/hand/right/input/select/click");
                bindings[currBinding++] = ActionSuggestedBinding(buttonAAction, "/user/hand/left/input/menu/click");
                bindings[currBinding++] = ActionSuggestedBinding(buttonXAction, "/user/hand/right/input/menu/click");
                bindings[currBinding++] = ActionSuggestedBinding(vibrateLeftFeedback, "/user/hand/left/output/haptic");
                bindings[currBinding++] = ActionSuggestedBinding(vibrateRightFeedback, "/user/hand/right/output/haptic");
                bindings[currBinding++] = ActionSuggestedBinding(handPoseLeftAction, "/user/hand/left/input/aim/pose");
                bindings[currBinding++] = ActionSuggestedBinding(handPoseRightAction, "/user/hand/right/input/aim/pose");
            }
        }

        XrInteractionProfileSuggestedBinding suggestedBindings = {};
        suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        suggestedBindings.next = NULL;
        suggestedBindings.interactionProfile = interactionProfilePath;
        suggestedBindings.suggestedBindings = bindings;
        suggestedBindings.countSuggestedBindings = currBinding;
        OXR(xrSuggestInteractionProfileBindings(gAppState.Instance, &suggestedBindings));

        // Enumerate actions
        XrPath actionPathsBuffer[32];
        char stringBuffer[256];
        XrAction actionsToEnumerate[] = {
                indexLeftAction,
                indexRightAction,
                menuAction,
                buttonAAction,
                buttonBAction,
                buttonXAction,
                buttonYAction,
                gripLeftAction,
                gripRightAction,
                moveOnLeftJoystickAction,
                moveOnRightJoystickAction,
                thumbstickLeftClickAction,
                thumbstickRightClickAction,
                vibrateLeftFeedback,
                vibrateRightFeedback,
                handPoseLeftAction,
                handPoseRightAction
        };
        for (size_t i = 0; i < sizeof(actionsToEnumerate) / sizeof(actionsToEnumerate[0]); ++i) {
            XrBoundSourcesForActionEnumerateInfo enumerateInfo = {};
            enumerateInfo.type = XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO;
            enumerateInfo.next = NULL;
            enumerateInfo.action = actionsToEnumerate[i];

            // Get Count
            uint32_t countOutput = 0;
            OXR(xrEnumerateBoundSourcesForAction(
                    gAppState.Session, &enumerateInfo, 0 /* request size */, &countOutput, NULL));
            ALOGV(
                    "xrEnumerateBoundSourcesForAction action=%lld count=%u",
                    (long long)enumerateInfo.action,
                    countOutput);

            if (countOutput < 32) {
                OXR(xrEnumerateBoundSourcesForAction(
                        gAppState.Session, &enumerateInfo, 32, &countOutput, actionPathsBuffer));
                for (uint32_t a = 0; a < countOutput; ++a) {
                    XrInputSourceLocalizedNameGetInfo nameGetInfo = {};
                    nameGetInfo.type = XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO;
                    nameGetInfo.next = NULL;
                    nameGetInfo.sourcePath = actionPathsBuffer[a];
                    nameGetInfo.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                                  XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                                  XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;

                    uint32_t stringCount = 0u;
                    OXR(xrGetInputSourceLocalizedName(
                            gAppState.Session, &nameGetInfo, 0, &stringCount, NULL));
                    if (stringCount < 256) {
                        OXR(xrGetInputSourceLocalizedName(
                                gAppState.Session, &nameGetInfo, 256, &stringCount, stringBuffer));
                        char pathStr[256];
                        uint32_t strLen = 0;
                        OXR(xrPathToString(
                                gAppState.Instance,
                                actionPathsBuffer[a],
                                (uint32_t)sizeof(pathStr),
                                &strLen,
                                pathStr));
                        ALOGV(
                                "  -> path = %lld `%s` -> `%s`",
                                (long long)actionPathsBuffer[a],
                                pathStr,
                                stringBuffer);
                    }
                }
            }
        }
    }

    // Attach to session
    XrSessionActionSetsAttachInfo attachInfo = {};
    attachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
    attachInfo.next = NULL;
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &runningActionSet;
    OXR(xrAttachSessionActionSets(gAppState.Session, &attachInfo));

    inputInitialized = true;
}

void TBXR_SyncActions( void )
{
	// sync action data
	XrActiveActionSet activeActionSet = {};
	activeActionSet.actionSet = runningActionSet;
	activeActionSet.subactionPath = XR_NULL_PATH;

	XrActionsSyncInfo syncInfo = {};
	syncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
	syncInfo.next = NULL;
	syncInfo.countActiveActionSets = 1;
	syncInfo.activeActionSets = &activeActionSet;
	OXR(xrSyncActions(gAppState.Session, &syncInfo));

	// query input action states
	XrActionStateGetInfo getInfo = {};
	getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
	getInfo.next = NULL;
	getInfo.subactionPath = XR_NULL_PATH;
}

void TBXR_UpdateControllers( )
{
	TBXR_SyncActions();

	//get controller poses
	XrAction controller[] = {handPoseLeftAction, handPoseRightAction};
	XrPath subactionPath[] = {leftHandPath, rightHandPath};
	XrSpace controllerSpace[] = {leftControllerAimSpace, rightControllerAimSpace};
	for (int i = 0; i < 2; i++) {
		if (ActionPoseIsActive(controller[i], subactionPath[i])) {
			XrSpaceVelocity vel = {};
			vel.type = XR_TYPE_SPACE_VELOCITY;
			XrSpaceLocation loc = {};
			loc.type = XR_TYPE_SPACE_LOCATION;
			loc.next = &vel;
			OXR(xrLocateSpace(controllerSpace[i], gAppState.CurrentSpace, gAppState.PredictedDisplayTime, &loc));

			gAppState.TrackedController[i].Active = (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
			gAppState.TrackedController[i].Pose = loc.pose;
			gAppState.TrackedController[i].Velocity = vel;
		} else {
			ovrTrackedController_Clear(&gAppState.TrackedController[i]);
		}
	}

	leftRemoteTracking_new = gAppState.TrackedController[0];
	rightRemoteTracking_new = gAppState.TrackedController[1];


	memset(&leftTrackedRemoteState_new, 0, sizeof leftTrackedRemoteState_new);
	memset(&rightTrackedRemoteState_new, 0, sizeof rightTrackedRemoteState_new);

	//button mapping
	if (GetActionStateBoolean(menuAction).currentState) leftTrackedRemoteState_new.Buttons |= xrButton_Enter;
	if (GetActionStateBoolean(buttonXAction).currentState) leftTrackedRemoteState_new.Buttons |= xrButton_X;
	if (GetActionStateBoolean(buttonYAction).currentState) leftTrackedRemoteState_new.Buttons |= xrButton_Y;
	leftTrackedRemoteState_new.GripTrigger = GetActionStateFloat(gripLeftAction).currentState;
	if (leftTrackedRemoteState_new.GripTrigger > 0.5f) leftTrackedRemoteState_new.Buttons |= xrButton_GripTrigger;
	if (GetActionStateBoolean(thumbstickLeftClickAction).currentState) leftTrackedRemoteState_new.Buttons |= xrButton_LThumb;

	if (GetActionStateBoolean(buttonAAction).currentState) rightTrackedRemoteState_new.Buttons |= xrButton_A;
	if (GetActionStateBoolean(buttonBAction).currentState) rightTrackedRemoteState_new.Buttons |= xrButton_B;
	rightTrackedRemoteState_new.GripTrigger = GetActionStateFloat(gripRightAction).currentState;
	if (rightTrackedRemoteState_new.GripTrigger > 0.5f) rightTrackedRemoteState_new.Buttons |= xrButton_GripTrigger;
	if (GetActionStateBoolean(thumbstickRightClickAction).currentState) rightTrackedRemoteState_new.Buttons |= xrButton_RThumb;

	//index finger click
	if (GetActionStateBoolean(indexLeftAction).currentState) leftTrackedRemoteState_new.Buttons |= xrButton_Trigger;
	if (GetActionStateBoolean(indexRightAction).currentState) rightTrackedRemoteState_new.Buttons |= xrButton_Trigger;

	//thumbstick
	XrActionStateVector2f moveJoystickState;
	moveJoystickState = GetActionStateVector2(moveOnLeftJoystickAction);
	leftTrackedRemoteState_new.Joystick.x = moveJoystickState.currentState.x;
	leftTrackedRemoteState_new.Joystick.y = moveJoystickState.currentState.y;

	moveJoystickState = GetActionStateVector2(moveOnRightJoystickAction);
	rightTrackedRemoteState_new.Joystick.x = moveJoystickState.currentState.x;
	rightTrackedRemoteState_new.Joystick.y = moveJoystickState.currentState.y;
}

void TBXR_Vibrate( int duration, int chan, float intensity )
{
    for (int i = 0; i < 2; ++i)
    {
        int channel = 1-i;
        if ((i + 1) & chan)
        {
            if (vibration_channel_duration[channel] > 0.0f)
                return;

            if (vibration_channel_duration[channel] == -1.0f && duration != 0.0f)
                return;

            vibration_channel_duration[channel] = duration;
            vibration_channel_intensity[channel] = intensity;
        }
    }
}

void TBXR_ProcessHaptics() {
    static float lastFrameTime = 0.0f;
    float timestamp = (float)(TBXR_GetTimeInMilliSeconds());
    float frametime = timestamp - lastFrameTime;
    lastFrameTime = timestamp;

    for (int i = 0; i < 2; ++i) {
        if (vibration_channel_duration[i] > 0.0f ||
            vibration_channel_duration[i] == -1.0f) {

            // fire haptics using output action
            XrHapticVibration vibration = {};
            vibration.type = XR_TYPE_HAPTIC_VIBRATION;
            vibration.next = NULL;
            vibration.amplitude = vibration_channel_intensity[i];
            vibration.duration = ToXrTime(vibration_channel_duration[i]);
            vibration.frequency = 3000;
            XrHapticActionInfo hapticActionInfo = {};
            hapticActionInfo.type = XR_TYPE_HAPTIC_ACTION_INFO;
            hapticActionInfo.next = NULL;
            hapticActionInfo.action = i == 0 ? vibrateLeftFeedback : vibrateRightFeedback;
            OXR(xrApplyHapticFeedback(gAppState.Session, &hapticActionInfo, (const XrHapticBaseHeader*)&vibration));

            if (vibration_channel_duration[i] != -1.0f) {
                vibration_channel_duration[i] -= frametime;

                if (vibration_channel_duration[i] < 0.0f) {
                    vibration_channel_duration[i] = 0.0f;
                    vibration_channel_intensity[i] = 0.0f;
                }
            }
        } else {
            // Stop haptics
            XrHapticActionInfo hapticActionInfo = {};
            hapticActionInfo.type = XR_TYPE_HAPTIC_ACTION_INFO;
            hapticActionInfo.next = NULL;
            hapticActionInfo.action = i == 0 ? vibrateLeftFeedback : vibrateRightFeedback;
            OXR(xrStopHapticFeedback(gAppState.Session, &hapticActionInfo));
        }
    }
}
#endif
