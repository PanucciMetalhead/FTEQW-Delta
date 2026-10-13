#include "quakedef.h"

#include <SDL3/SDL.h>


extern SDL_Window *sdlwindow;

qboolean mouseactive;
extern qboolean mouseusedforgui;
extern qboolean vid_isfullscreen;

#define HAVE_SDL_TEXTINPUT
cvar_t sys_osk = CVARD("sys_osk", "0", "Enables support for text input. This will be ignored when the console has focus, but gamecode may end up with composition boxes appearing.");

void IN_ActivateMouse(void)
{
	if (mouseactive)
		return;
	if (!vid.activeapp)
		return;

	mouseactive = true;
	SDL_HideCursor();
	SDL_SetWindowRelativeMouseMode(sdlwindow, true);
	SDL_SetWindowMouseGrab(sdlwindow, true);
}

void IN_DeactivateMouse(void)
{
	if (!mouseactive)
		return;

	mouseactive = false;
	SDL_ShowCursor();
	SDL_SetWindowRelativeMouseMode(sdlwindow, false);
	SDL_SetWindowMouseGrab(sdlwindow, false);
}

#define MAX_FINGERS 16 //zomg! mutant!
static struct sdlfinger_s
{
	qboolean active;
	SDL_JoystickID jid;
	SDL_TouchID tid;
	SDL_FingerID fid;
} sdlfinger[MAX_FINGERS];
//sanitizes sdl fingers into touch events that our engine can eat.
//we don't really deal with different devices, we just munge the lot into a single thing (allowing fingers to be tracked, at least if splitscreen isn't active).
static uint32_t SDL_GiveFinger(SDL_JoystickID jid, SDL_TouchID tid, SDL_FingerID fid, qboolean fingerraised)
{
	uint32_t f;
	for (f = 0; f < countof(sdlfinger); f++)
	{
		if (sdlfinger[f].active)
		{
			if (sdlfinger[f].jid == jid && sdlfinger[f].tid == tid && sdlfinger[f].fid == fid)
			{
				sdlfinger[f].active = !fingerraised;
				return f;
			}
		}
	}
	for (f = 0; f < countof(sdlfinger); f++)
	{
		if (!sdlfinger[f].active)
		{
			sdlfinger[f].active = !fingerraised;
			sdlfinger[f].jid = jid;
			sdlfinger[f].tid = tid;
			sdlfinger[f].fid = fid;
			return f;
		}
	}
	return f;
}

#define MAX_JOYSTICKS 16
#ifndef MAXJOYAXIS
#define MAXJOYAXIS 6
#endif

static struct
{
	int qaxis;
	keynum_t pos, neg;
} gpaxismap[] =
{
	{/*SDL_CONTROLLER_AXIS_LEFTX*/			GPAXIS_LT_RIGHT,	K_GP_LEFT_THUMB_RIGHT,	K_GP_LEFT_THUMB_LEFT},
	{/*SDL_CONTROLLER_AXIS_LEFTY*/			GPAXIS_LT_DOWN,		K_GP_LEFT_THUMB_DOWN,	K_GP_LEFT_THUMB_UP},
	{/*SDL_CONTROLLER_AXIS_RIGHTX*/			GPAXIS_RT_RIGHT,	K_GP_RIGHT_THUMB_RIGHT,	K_GP_RIGHT_THUMB_LEFT},
	{/*SDL_CONTROLLER_AXIS_RIGHTY*/			GPAXIS_RT_DOWN,		K_GP_RIGHT_THUMB_DOWN,	K_GP_RIGHT_THUMB_UP},
	{/*SDL_CONTROLLER_AXIS_TRIGGERLEFT*/	GPAXIS_LT_TRIGGER,	K_GP_LEFT_TRIGGER,		0},
	{/*SDL_CONTROLLER_AXIS_TRIGGERRIGHT*/	GPAXIS_RT_TRIGGER,	K_GP_RIGHT_TRIGGER,		0},
};
static const int gpbuttonmap[] =
{
	K_GP_A,
	K_GP_B,
	K_GP_X,
	K_GP_Y,
	K_GP_BACK,
	K_GP_GUIDE,
	K_GP_START,
	K_GP_LEFT_STICK,
	K_GP_RIGHT_STICK,
	K_GP_LEFT_SHOULDER,
	K_GP_RIGHT_SHOULDER,
	K_GP_DPAD_UP,
	K_GP_DPAD_DOWN,
	K_GP_DPAD_LEFT,
	K_GP_DPAD_RIGHT,
	K_GP_MISC1,
	K_GP_PADDLE1,
	K_GP_PADDLE2,
	K_GP_PADDLE3,
	K_GP_PADDLE4,
	K_GP_TOUCHPAD
};

static struct sdljoy_s
{
	//fte doesn't distinguish between joysticks and controllers.
	//in sdl, controllers are some glorified version of joysticks apparently.
	char *devname;
	SDL_Joystick *joystick;
	SDL_Gamepad *controller;
	SDL_JoystickID id;
	unsigned int qdevid;

	//axis junk
	unsigned int axistobuttonp;
	unsigned int axistobuttonn;
	float repeattime[countof(gpaxismap)];

	//button junk
	unsigned int buttonheld;
	float buttonrepeat[countof(gpbuttonmap)];
} sdljoy[MAX_JOYSTICKS]; 

static cvar_t joy_only = CVARD("joyonly", "0", "If true, treats \"game controllers\" as regular joysticks.\nMust be set at startup.");

//the enumid is the value for the open function rather than the working id.
static void J_AllocateDevID(struct sdljoy_s *joy)
{
	extern cvar_t in_skipplayerone;
	unsigned int id = (in_skipplayerone.ival?1:0), j;
	for (j = 0; j < MAX_JOYSTICKS;)
	{
		if (sdljoy[j++].qdevid == id)
		{
			j = 0;
			id++;
		}
	}

	joy->qdevid = id;

	if (joy->controller)
	{
		//enable some sensors if they're there. because we can.
		if (SDL_GamepadHasSensor(joy->controller, SDL_SENSOR_ACCEL))
			SDL_SetGamepadSensorEnabled(joy->controller, SDL_SENSOR_ACCEL, true);
		if (SDL_GamepadHasSensor(joy->controller, SDL_SENSOR_GYRO))
			SDL_SetGamepadSensorEnabled(joy->controller, SDL_SENSOR_GYRO, true);
	}
}
static void J_ControllerAdded(int enumid)
{
	const char *cname;
	int i;

	if (joy_only.ival)
		return;

	for (i = 0; i < MAX_JOYSTICKS; i++)
		if (sdljoy[i].controller == NULL && sdljoy[i].joystick == NULL)
			break;
	if (i == MAX_JOYSTICKS)
		return;

	sdljoy[i].controller = SDL_OpenGamepad(enumid);
	if (!sdljoy[i].controller)
		return;
	sdljoy[i].joystick = SDL_GetGamepadJoystick(sdljoy[i].controller);
	sdljoy[i].id = SDL_GetJoystickID(sdljoy[i].joystick);

	cname = SDL_GetGamepadName(sdljoy[i].controller);
	if (!cname)
		cname = "Unknown Controller";
	Con_Printf("Found new controller (%i): %s\n", i, cname);
	sdljoy[i].devname = Z_StrDup(cname);
	sdljoy[i].qdevid = DEVID_UNSET;
}
static void J_JoystickAdded(int enumid)
{
	const char *cname;
	int i;
	for (i = 0; i < MAX_JOYSTICKS; i++)
		if (sdljoy[i].joystick == NULL && sdljoy[i].controller == NULL)
			break;
	if (i == MAX_JOYSTICKS)
		return;

	if (!joy_only.ival  &&  SDL_IsGamepad(enumid))	//if its reported via the gamecontroller api then use that instead. don't open it twice.
		return;

	sdljoy[i].joystick = SDL_OpenJoystick(enumid);
	if (!sdljoy[i].joystick)
		return;
	sdljoy[i].id = SDL_GetJoystickID(sdljoy[i].joystick);

	cname = SDL_GetJoystickName(sdljoy[i].joystick);
	if (!cname)
		cname = "Unknown Joystick";
	Con_Printf("Found new joystick (%i): %s\n", i, cname);
	sdljoy[i].qdevid = DEVID_UNSET;
}
static struct sdljoy_s *J_DevId(SDL_JoystickID jid)
{
	int i;
	for (i = 0; i < MAX_JOYSTICKS; i++)
		if (sdljoy[i].joystick && sdljoy[i].id == jid)
			return &sdljoy[i];
	return NULL;
}
static void J_ControllerAxis(SDL_JoystickID jid, int axis, int value)
{
	struct sdljoy_s *joy = J_DevId(jid);

	if (joy->qdevid == DEVID_UNSET)
	{
		if (abs(value) < 0x4000)
			return;	//has to be big enough to handle any generous dead zone.
		J_AllocateDevID(joy);
	}

	if (joy && axis < countof(gpaxismap) && joy->qdevid != DEVID_UNSET)
	{
		if (value > 0x4000 && gpaxismap[axis].pos)
		{
			if (!(joy->axistobuttonp & (1u<<axis)))
			{
				IN_KeyEvent(joy->qdevid, true, gpaxismap[axis].pos, 0);
				joy->repeattime[axis] = 1.0;
			}
			joy->axistobuttonp |= 1u<<axis;
		}
		else if (joy->axistobuttonp & (1u<<axis))
		{
			IN_KeyEvent(joy->qdevid, false, gpaxismap[axis].pos, 0);
			joy->axistobuttonp &= ~(1u<<axis);
		}

		if (value < -0x4000 && gpaxismap[axis].neg)
		{
			if (!(joy->axistobuttonn & (1u<<axis)))
			{
				IN_KeyEvent(joy->qdevid, true, gpaxismap[axis].neg, 0);
				joy->repeattime[axis] = 1.0;
			}
			joy->axistobuttonn |= 1u<<axis;
		}
		else if (joy->axistobuttonn & (1u<<axis))
		{
			IN_KeyEvent(joy->qdevid, false, gpaxismap[axis].neg, 0);
			joy->axistobuttonn &= ~(1u<<axis);
		}

		IN_JoystickAxisEvent(joy->qdevid, gpaxismap[axis].qaxis, value / 32767.0);
	}
}
static void J_JoystickAxis(SDL_JoystickID jid, int axis, int value)
{
	struct sdljoy_s *joy = J_DevId(jid);

	if (joy->qdevid == DEVID_UNSET)
	{
		if (abs(value) < 0x1000)
			return;
		J_AllocateDevID(joy);
	}

	if (joy && !joy->controller && axis < MAXJOYAXIS && joy->qdevid != DEVID_UNSET)
		IN_JoystickAxisEvent(joy->qdevid, axis, value / 32767.0);
}
//we don't do hats and balls and stuff.
static void J_ControllerButton(SDL_JoystickID jid, int button, qboolean pressed)
{
	//controllers have reliable button maps.
	//but that doesn't meant that fte has specific k_ names for those buttons, but the mapping should be reliable, at least until they get mapped to proper k_ values.
	struct sdljoy_s *joy = J_DevId(jid);
	if (joy && button < countof(gpbuttonmap))
	{
		if (joy->qdevid == DEVID_UNSET)
		{
			if (!pressed)
				return;
			J_AllocateDevID(joy);
		}
		if (pressed)
			joy->buttonheld |= (1u<<button);
		else
			joy->buttonheld &= ~(1u<<button);
		IN_KeyEvent(joy->qdevid, pressed, gpbuttonmap[button], 0);
		joy->buttonrepeat[button] = 1.0;
	}
}
static void J_JoystickButton(SDL_JoystickID jid, int button, qboolean pressed)
{
	//generic joysticks have no specific mappings. they're really random like that.
	static const int buttonmap[] = {
		K_JOY1,
		K_JOY2,
		K_JOY3,
		K_JOY4,
		K_JOY5,
		K_JOY6,
		K_JOY7,
		K_JOY8,
		K_JOY9,
		K_JOY10,
		K_JOY11,
		K_JOY12,
		K_JOY13,
		K_JOY14,
		K_JOY15,
		K_JOY16,
		K_JOY17,
		K_JOY18,
		K_JOY19,
		K_JOY20,
		K_JOY21,
		K_JOY22,
		K_JOY23,
		K_JOY24,
		K_JOY25,
		K_JOY26,
		K_JOY27,
		K_JOY28,
		K_JOY29,
		K_JOY30,
		K_JOY31,
		K_JOY32,
		K_AUX1,
		K_AUX2,
		K_AUX3,
		K_AUX4,
		K_AUX5,
		K_AUX6,
		K_AUX7,
		K_AUX8,
		K_AUX9,
		K_AUX10,
		K_AUX11,
		K_AUX12,
		K_AUX13,
		K_AUX14,
		K_AUX15,
		K_AUX16
	};

	struct sdljoy_s *joy = J_DevId(jid);
	if (joy && !joy->controller && button < sizeof(buttonmap)/sizeof(buttonmap[0]))
	{
		if (joy->qdevid == DEVID_UNSET)
		{
			if (!pressed)
				return;
			J_AllocateDevID(joy);
		}
		IN_KeyEvent(joy->qdevid, pressed, buttonmap[button], 0);
	}
}

enum controllertype_e INS_GetControllerType(int id)
{
	int i;
	for (i = 0; i < MAX_JOYSTICKS; i++)
	{
		if (sdljoy[i].qdevid == id)
		{
			switch(SDL_GetGamepadTypeForID(sdljoy[i].id))
			{	//sdl reports whether it has touchpads etc. we only really report a value for button icons.
			case SDL_GAMEPAD_TYPE_STANDARD:	//I guess microsoft's xinput won.
			case SDL_GAMEPAD_TYPE_XBOX360:
			case SDL_GAMEPAD_TYPE_XBOXONE:
				return CONTROLLER_XBOX;
			case SDL_GAMEPAD_TYPE_PS3:
			case SDL_GAMEPAD_TYPE_PS4:
			case SDL_GAMEPAD_TYPE_PS5:
				return CONTROLLER_PLAYSTATION;
			case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
			case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
			case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
			case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
				return CONTROLLER_NINTENDO;
			case SDL_GAMEPAD_TYPE_UNKNOWN:
			case SDL_GAMEPAD_TYPE_COUNT:
			default:
				return CONTROLLER_UNKNOWN;
			}
		}
	}
	return 0;
}
void INS_Rumble(int id, quint16_t amp_low, quint16_t amp_high, quint32_t duration)
{
	int i;
	if (duration > 10000)
		duration = 10000;

	for (i = 0; i < MAX_JOYSTICKS; i++)
	{
		if (sdljoy[i].qdevid == id)
		{
			SDL_RumbleGamepad(SDL_GetGamepadFromID(sdljoy[i].id), amp_low, amp_high, duration);
			return;
		}
	}
}

void INS_RumbleTriggers(int id, quint16_t left, quint16_t right, quint32_t duration)
{
	int i;
	if (duration > 10000)
		duration = 10000;

	for (i = 0; i < MAX_JOYSTICKS; i++)
	{
		if (sdljoy[i].qdevid == id)
		{
			SDL_RumbleGamepadTriggers(SDL_GetGamepadFromID(sdljoy[i].id), left, right, duration);
			return;
		}
	}
}

void INS_SetLEDColor(int id, vec3_t color)
{
	int i;
	/* maybe we'll eventually get sRGB LEDs */
	color[0] *= 255.0f;
	color[1] *= 255.0f;
	color[2] *= 255.0f;

	for (i = 0; i < MAX_JOYSTICKS; i++)
	{
		if (sdljoy[i].qdevid == id)
		{
			SDL_SetGamepadLED(SDL_GetGamepadFromID(sdljoy[i].id), (uint8_t)color[0], (uint8_t)color[1], (uint8_t)color[2]);
			return;
		}
	}
}

void INS_SetTriggerFX(int id, const void *data, size_t size)
{
	int i;
	for (i = 0; i < MAX_JOYSTICKS; i++)
	{
		if (sdljoy[i].qdevid == id)
		{
			SDL_SendGamepadEffect(SDL_GetGamepadFromID(sdljoy[i].id), &data, size);
			return;
		}
	}
}

static void J_ControllerTouchPad(SDL_JoystickID jid, int pad, int finger, int fingerstate, float x, float y, float pressure)
{
	//FIXME: forgets which seat its meant to be controlling.
	uint32_t thefinger = SDL_GiveFinger(jid, pad, finger, fingerstate<0);
	x *= vid.pixelwidth;
	y *= vid.pixelheight;
	IN_MouseMove(thefinger, true, x, y, 0, pressure);
	if (fingerstate)
		IN_KeyEvent(thefinger, fingerstate>0, K_GP_TOUCHPAD, 0);
}
static void J_ControllerSensor(SDL_JoystickID jid, SDL_SensorType sensor, float *data)
{
	struct sdljoy_s *joy = J_DevId(jid);
	if (!joy)
		return;
	//don't assign an id here. wait for a button.
	if (joy->qdevid == DEVID_UNSET)
		return;

	switch(sensor)
	{
	case SDL_SENSOR_ACCEL:
		IN_Accelerometer(joy->qdevid, data[0], data[1], data[2]);
		break;
	case SDL_SENSOR_GYRO:
		IN_Gyroscope(joy->qdevid, data[0], data[1], data[2]);
		break;
/*#if 1
	case SDL_SENSOR_ACCEL_L:
	case SDL_SENSOR_ACCEL_R:
	case SDL_SENSOR_GYRO_L:
	case SDL_SENSOR_GYRO_R:
#endif*/
	case SDL_SENSOR_INVALID:
	case SDL_SENSOR_UNKNOWN:
	default:
		break;
	}
}

static void J_Kill(SDL_JoystickID jid, qboolean verbose)
{
	int i;
	struct sdljoy_s *joy = J_DevId(jid);

	if (!joy)
		return;

	//make sure all the axis are nulled out, to avoid surprises.
	if (joy->qdevid != DEVID_UNSET)
	{
		for (i = 0; i < 6; i++)
			IN_JoystickAxisEvent(joy->qdevid, i, 0);
		if (joy->controller)
		{
			for (i = 0; i < 32; i++)
				J_ControllerButton(jid, i, false);
		}
		else
		{
			for (i = 0; i < 32; i++)
				J_JoystickButton(jid, i, false);
		}
	}

	if (joy->controller)
	{
		Con_Printf("Controller unplugged(%i): %s\n", (int)(joy - sdljoy), joy->devname);
		SDL_CloseGamepad(joy->controller);
	}
	else
	{
		Con_Printf("Joystick unplugged(%i): %s\n", (int)(joy - sdljoy), joy->devname);
		SDL_CloseJoystick(joy->joystick);
	}
	joy->controller = NULL;
	joy->joystick = NULL;
	Z_Free(joy->devname);
	joy->devname = NULL;
	joy->qdevid = DEVID_UNSET;
}
static void J_KillAll(void)
{
	int i;
	for (i = 0; i < MAX_JOYSTICKS; i++)
		J_Kill(sdljoy[i].id, false);
}

//use SDL_GetKeyName(SDL_GetKeyFromScancode(quaketosdl[qkey])) for keybinds menu
unsigned int MySDL_MapScancode(SDL_Scancode sdlscancode)
{
	switch(sdlscancode)
	{
	case SDL_SCANCODE_A:
	case SDL_SCANCODE_B:
	case SDL_SCANCODE_C:
	case SDL_SCANCODE_D:
	case SDL_SCANCODE_E:
	case SDL_SCANCODE_F:
	case SDL_SCANCODE_G:
	case SDL_SCANCODE_H:
	case SDL_SCANCODE_I:
	case SDL_SCANCODE_J:
	case SDL_SCANCODE_K:
	case SDL_SCANCODE_L:
	case SDL_SCANCODE_M:
	case SDL_SCANCODE_N:
	case SDL_SCANCODE_O:
	case SDL_SCANCODE_P:
	case SDL_SCANCODE_Q:
	case SDL_SCANCODE_R:
	case SDL_SCANCODE_S:
	case SDL_SCANCODE_T:
	case SDL_SCANCODE_U:
	case SDL_SCANCODE_V:
	case SDL_SCANCODE_W:
	case SDL_SCANCODE_X:
	case SDL_SCANCODE_Y:
	case SDL_SCANCODE_Z:			return 'a' + sdlscancode-SDL_SCANCODE_A;
	case SDL_SCANCODE_1:
	case SDL_SCANCODE_2:
	case SDL_SCANCODE_3:
	case SDL_SCANCODE_4:
	case SDL_SCANCODE_5:
	case SDL_SCANCODE_6:
	case SDL_SCANCODE_7:
	case SDL_SCANCODE_8:
	case SDL_SCANCODE_9:			return '1' + sdlscancode-SDL_SCANCODE_1;
	case SDL_SCANCODE_0:			return '0';
	case SDL_SCANCODE_RETURN:		return K_ENTER;
	case SDL_SCANCODE_ESCAPE:		return K_ESCAPE;
	case SDL_SCANCODE_BACKSPACE:	return K_BACKSPACE;
	case SDL_SCANCODE_TAB:			return K_TAB;
	case SDL_SCANCODE_SPACE:		return K_SPACE;
	case SDL_SCANCODE_MINUS:		return '-';
	case SDL_SCANCODE_EQUALS:		return '=';
	case SDL_SCANCODE_LEFTBRACKET:	return '[';
	case SDL_SCANCODE_RIGHTBRACKET:	return ']';
	case SDL_SCANCODE_BACKSLASH:	return '#';
	case SDL_SCANCODE_NONUSHASH:	return '#';
	case SDL_SCANCODE_SEMICOLON:	return ';';
	case SDL_SCANCODE_APOSTROPHE:	return '\'';
	case SDL_SCANCODE_GRAVE:		return '`';
	case SDL_SCANCODE_COMMA:		return ',';
	case SDL_SCANCODE_PERIOD:		return '.';
	case SDL_SCANCODE_SLASH:		return '/';
	case SDL_SCANCODE_CAPSLOCK:		return K_CAPSLOCK;
	case SDL_SCANCODE_F1:			return K_F1;
	case SDL_SCANCODE_F2:			return K_F2;
	case SDL_SCANCODE_F3:			return K_F3;
	case SDL_SCANCODE_F4:			return K_F4;
	case SDL_SCANCODE_F5:			return K_F5;
	case SDL_SCANCODE_F6:			return K_F6;
	case SDL_SCANCODE_F7:			return K_F7;
	case SDL_SCANCODE_F8:			return K_F8;
	case SDL_SCANCODE_F9:			return K_F9;
	case SDL_SCANCODE_F10:			return K_F10;
	case SDL_SCANCODE_F11:			return K_F11;
	case SDL_SCANCODE_F12:			return K_F12;
	case SDL_SCANCODE_PRINTSCREEN:	return K_PRINTSCREEN;
	case SDL_SCANCODE_SCROLLLOCK:	return K_SCRLCK;
	case SDL_SCANCODE_PAUSE:		return K_PAUSE;
	case SDL_SCANCODE_INSERT:		return K_INS;
	case SDL_SCANCODE_HOME:			return K_HOME;
	case SDL_SCANCODE_PAGEUP:		return K_PGUP;
	case SDL_SCANCODE_DELETE:		return K_DEL;
	case SDL_SCANCODE_END:			return K_END;
	case SDL_SCANCODE_PAGEDOWN:		return K_PGDN;
	case SDL_SCANCODE_RIGHT:		return K_RIGHTARROW;
	case SDL_SCANCODE_LEFT:			return K_LEFTARROW;
	case SDL_SCANCODE_DOWN:			return K_DOWNARROW;
	case SDL_SCANCODE_UP:			return K_UPARROW;
	case SDL_SCANCODE_NUMLOCKCLEAR:	return K_KP_NUMLOCK;
	case SDL_SCANCODE_KP_DIVIDE:	return K_KP_SLASH;
	case SDL_SCANCODE_KP_MULTIPLY:	return K_KP_STAR;
	case SDL_SCANCODE_KP_MINUS:		return K_KP_MINUS;
	case SDL_SCANCODE_KP_PLUS:		return K_KP_PLUS;
	case SDL_SCANCODE_KP_ENTER:		return K_KP_ENTER;
	case SDL_SCANCODE_KP_1:			return K_KP_END;
	case SDL_SCANCODE_KP_2:			return K_KP_DOWNARROW;
	case SDL_SCANCODE_KP_3:			return K_KP_PGDN;
	case SDL_SCANCODE_KP_4:			return K_KP_LEFTARROW;
	case SDL_SCANCODE_KP_5:			return K_KP_5;
	case SDL_SCANCODE_KP_6:			return K_KP_RIGHTARROW;
	case SDL_SCANCODE_KP_7:			return K_KP_HOME;
	case SDL_SCANCODE_KP_8:			return K_KP_UPARROW;
	case SDL_SCANCODE_KP_9:			return K_KP_PGUP;
	case SDL_SCANCODE_KP_0:			return K_KP_INS;
	case SDL_SCANCODE_KP_PERIOD:	return K_KP_DEL;
	case SDL_SCANCODE_NONUSBACKSLASH:	return '\\';
	case SDL_SCANCODE_APPLICATION:		return K_APP;
	case SDL_SCANCODE_POWER:			return K_POWER;
	case SDL_SCANCODE_KP_EQUALS:		return K_KP_EQUALS;
	case SDL_SCANCODE_F13:				return K_F13;
	case SDL_SCANCODE_F14:				return K_F14;
	case SDL_SCANCODE_F15:				return K_F15;
//	case SDL_SCANCODE_F16:				return K_F16;
//	case SDL_SCANCODE_F17:				return K_F17;
//	case SDL_SCANCODE_F18:				return K_F18;
//	case SDL_SCANCODE_F19:				return K_F19;
//	case SDL_SCANCODE_F20:				return K_F20;
//	case SDL_SCANCODE_F21:				return K_F21;
//	case SDL_SCANCODE_F22:				return K_F22;
//	case SDL_SCANCODE_F23:				return K_F23;
//	case SDL_SCANCODE_F24:				return K_F24;
//	case SDL_SCANCODE_EXECUTE:			return K_;
//	case SDL_SCANCODE_HELP:				return K_;
//	case SDL_SCANCODE_MENU:				return K_;
//	case SDL_SCANCODE_SELECT:			return K_;
//	case SDL_SCANCODE_STOP:				return K_;
//	case SDL_SCANCODE_AGAIN:			return K_;
//	case SDL_SCANCODE_UNDO:				return K_;
//	case SDL_SCANCODE_CUT:				return K_;
//	case SDL_SCANCODE_COPY:				return K_;
//	case SDL_SCANCODE_PASTE:			return K_;
//	case SDL_SCANCODE_FIND:				return K_;
//	case SDL_SCANCODE_MUTE:				return K_;
	case SDL_SCANCODE_VOLUMEUP:			return K_VOLUP;
	case SDL_SCANCODE_VOLUMEDOWN:		return K_VOLDOWN;
//	case SDL_SCANCODE_KP_COMMA:			return K_KP_;
//	case SDL_SCANCODE_KP_EQUALSAS400:	return K_KP_;
//	case SDL_SCANCODE_INTERNATIONAL1:	return K_;
//	case SDL_SCANCODE_INTERNATIONAL2:	return K_;
//	case SDL_SCANCODE_INTERNATIONAL3:	return K_;
//	case SDL_SCANCODE_INTERNATIONAL4:	return K_;
//	case SDL_SCANCODE_INTERNATIONAL5:	return K_;
//	case SDL_SCANCODE_INTERNATIONAL6:	return K_;
//	case SDL_SCANCODE_INTERNATIONAL7:	return K_;
//	case SDL_SCANCODE_INTERNATIONAL8:	return K_;
//	case SDL_SCANCODE_INTERNATIONAL9:	return K_;
//	case SDL_SCANCODE_LANG1:			return K_;
//	case SDL_SCANCODE_LANG2:			return K_;
//	case SDL_SCANCODE_LANG3:			return K_;
//	case SDL_SCANCODE_LANG4:			return K_;
//	case SDL_SCANCODE_LANG5:			return K_;
//	case SDL_SCANCODE_LANG6:			return K_;
//	case SDL_SCANCODE_LANG7:			return K_;
//	case SDL_SCANCODE_LANG8:			return K_;
//	case SDL_SCANCODE_LANG9:			return K_;
//	case SDL_SCANCODE_ALTERASE:			return K_;
//	case SDL_SCANCODE_SYSREQ:			return K_;
//	case SDL_SCANCODE_CANCEL:			return K_;
//	case SDL_SCANCODE_CLEAR:			return K_;
//	case SDL_SCANCODE_PRIOR:			return K_;
//	case SDL_SCANCODE_RETURN2:			return K_;
//	case SDL_SCANCODE_SEPARATOR:		return K_;
//	case SDL_SCANCODE_OUT:				return K_;
//	case SDL_SCANCODE_OPER:				return K_;
//	case SDL_SCANCODE_CLEARAGAIN:		return K_;
//	case SDL_SCANCODE_CRSEL:			return K_;
//	case SDL_SCANCODE_EXSEL:			return K_;
//	case SDL_SCANCODE_KP_00:				return K_KP_;
//	case SDL_SCANCODE_KP_000:				return K_KP_;
//	case SDL_SCANCODE_THOUSANDSSEPARATOR:	return K_KP_;
//	case SDL_SCANCODE_DECIMALSEPARATOR:		return K_KP_;
//	case SDL_SCANCODE_CURRENCYUNIT:			return K_KP_;
//	case SDL_SCANCODE_CURRENCYSUBUNIT:		return K_KP_;
//	case SDL_SCANCODE_KP_LEFTPAREN:			return K_KP_;
//	case SDL_SCANCODE_KP_RIGHTPAREN:		return K_KP_;
//	case SDL_SCANCODE_KP_LEFTBRACE:			return K_KP_;
//	case SDL_SCANCODE_KP_RIGHTBRACE:		return K_KP_;
//	case SDL_SCANCODE_KP_TAB:				return K_KP_;
//	case SDL_SCANCODE_KP_BACKSPACE:			return K_KP_;
//	case SDL_SCANCODE_KP_A:					return K_KP_;
//	case SDL_SCANCODE_KP_B:					return K_KP_;
//	case SDL_SCANCODE_KP_C:					return K_KP_;
//	case SDL_SCANCODE_KP_D:					return K_KP_;
//	case SDL_SCANCODE_KP_E:					return K_KP_;
//	case SDL_SCANCODE_KP_F:					return K_KP_;
//	case SDL_SCANCODE_KP_XOR:				return K_KP_;
//	case SDL_SCANCODE_KP_POWER:				return K_KP_;
//	case SDL_SCANCODE_KP_PERCENT:			return K_KP_;
//	case SDL_SCANCODE_KP_LESS:				return K_KP_;
//	case SDL_SCANCODE_KP_GREATER:			return K_KP_;
//	case SDL_SCANCODE_KP_AMPERSAND:			return K_KP_;
//	case SDL_SCANCODE_KP_DBLAMPERSAND:		return K_KP_;
//	case SDL_SCANCODE_KP_VERTICALBAR:		return K_KP_;
//	case SDL_SCANCODE_KP_DBLVERTICALBAR:	return K_KP_;
//	case SDL_SCANCODE_KP_COLON:				return K_KP_;
//	case SDL_SCANCODE_KP_HASH:				return K_KP_;
//	case SDL_SCANCODE_KP_SPACE:				return K_KP_;
//	case SDL_SCANCODE_KP_AT:				return K_KP_;
//	case SDL_SCANCODE_KP_EXCLAM:			return K_KP_;
//	case SDL_SCANCODE_KP_MEMSTORE:			return K_KP_;
//	case SDL_SCANCODE_KP_MEMRECALL:			return K_KP_;
//	case SDL_SCANCODE_KP_MEMCLEAR:			return K_KP_;
//	case SDL_SCANCODE_KP_MEMADD:			return K_KP_;
//	case SDL_SCANCODE_KP_MEMSUBTRACT:		return K_KP_;
//	case SDL_SCANCODE_KP_MEMMULTIPLY:		return K_KP_;
//	case SDL_SCANCODE_KP_MEMDIVIDE:			return K_KP_;
//	case SDL_SCANCODE_KP_PLUSMINUS:			return K_KP_;
//	case SDL_SCANCODE_KP_CLEAR:				return K_KP_;
//	case SDL_SCANCODE_KP_CLEARENTRY:		return K_KP_;
//	case SDL_SCANCODE_KP_BINARY:			return K_KP_;
//	case SDL_SCANCODE_KP_OCTAL:				return K_KP_;
//	case SDL_SCANCODE_KP_DECIMAL:			return K_KP_;
//	case SDL_SCANCODE_KP_HEXADECIMAL:		return K_KP_;
	case SDL_SCANCODE_LCTRL:				return K_LCTRL;
	case SDL_SCANCODE_LSHIFT:				return K_LSHIFT;
	case SDL_SCANCODE_LALT:					return K_LALT;
	case SDL_SCANCODE_LGUI:					return K_LWIN;
	case SDL_SCANCODE_RCTRL:				return K_RCTRL;
	case SDL_SCANCODE_RSHIFT:				return K_RSHIFT;
	case SDL_SCANCODE_RALT:					return K_RALT;
	case SDL_SCANCODE_RGUI:					return K_RWIN;
//	case SDL_SCANCODE_MODE:					return K_;
//	case SDL_SCANCODE_SLEEP:				return K_;
//	case SDL_SCANCODE_WAKE:					return K_;
//	case SDL_SCANCODE_CHANNEL_INCREMENT:	return K_;
//	case SDL_SCANCODE_CHANNEL_DECREMENT:	return K_;
//	case SDL_SCANCODE_MEDIA_PLAY:			return K_;
//	case SDL_SCANCODE_MEDIA_PAUSE:			return K_MM_TRACK_PLAYPAUSE;
//	case SDL_SCANCODE_MEDIA_RECORD:			return K_MM_;
//	case SDL_SCANCODE_MEDIA_FAST_FORWARD:	return K_MM_;
//	case SDL_SCANCODE_MEDIA_REWIND:			return K_MM_;
	case SDL_SCANCODE_MEDIA_NEXT_TRACK:		return K_MM_TRACK_NEXT;
	case SDL_SCANCODE_MEDIA_PREVIOUS_TRACK:	return K_MM_TRACK_PREV;
	case SDL_SCANCODE_MEDIA_STOP:			return K_MM_TRACK_STOP;
//	case SDL_SCANCODE_MEDIA_EJECT:			return K_MM_;
	case SDL_SCANCODE_MEDIA_PLAY_PAUSE:		return K_MM_TRACK_PLAYPAUSE;
//	case SDL_SCANCODE_MEDIA_SELECT:			return K_MM_;
//	case SDL_SCANCODE_AC_NEW:				return K_MM_;
//	case SDL_SCANCODE_AC_OPEN:				return K_MM_;
//	case SDL_SCANCODE_AC_CLOSE:				return K_MM_;
//	case SDL_SCANCODE_AC_EXIT:				return K_MM_;
//	case SDL_SCANCODE_AC_SAVE:				return K_MM_;
//	case SDL_SCANCODE_AC_PRINT:				return K_MM_;
//	case SDL_SCANCODE_AC_PROPERTIES:		return K_MM_;
//	case SDL_SCANCODE_AC_SEARCH:			return K_MM_;
	case SDL_SCANCODE_AC_HOME:				return K_MM_BROWSER_HOME;
	case SDL_SCANCODE_AC_BACK:				return K_MM_BROWSER_BACK;
	case SDL_SCANCODE_AC_FORWARD:			return K_MM_BROWSER_FORWARD;
	case SDL_SCANCODE_AC_STOP:				return K_MM_BROWSER_STOP;
	case SDL_SCANCODE_AC_REFRESH:			return K_MM_BROWSER_REFRESH;
	case SDL_SCANCODE_AC_BOOKMARKS:			return K_MM_BROWSER_FAVORITES;
//	case SDL_SCANCODE_SOFTLEFT:
//	case SDL_SCANCODE_SOFTRIGHT:
//	case SDL_SCANCODE_CALL:
//	case SDL_SCANCODE_ENDCALL:
	case SDL_SCANCODE_UNKNOWN:
	default:				return 0;
	}
}

/* stubbed */
qboolean INS_KeyToLocalName(int qkey, char *buf, size_t bufsize)
{
	const char *keyname;

	//too lazy to make+maintain a reverse MySDL_MapKey, so lets just make a reverse table with it instead.
	static qboolean stupidtabsetup;
	static SDL_Keycode stupidtable[K_MAX];
	if (!stupidtabsetup)
	{
		int i, k;
		for (i = 0; i < SDL_SCANCODE_COUNT; i++)
		{
			k = MySDL_MapScancode(i);
			if (k)
				stupidtable[k] = SDL_SCANCODE_TO_KEYCODE(i);
		}
		stupidtabsetup = true;
	}

	if (stupidtable[qkey])
		keyname = SDL_GetKeyName(stupidtable[qkey]);
	else if (qkey >= K_GP_DIAMOND_DOWN && qkey < K_GP_TOUCHPAD)
	{
		SDL_GamepadButton b;
		switch(qkey)
		{
		case K_GP_DIAMOND_DOWN:		b = SDL_GAMEPAD_BUTTON_SOUTH;			break;
		case K_GP_DIAMOND_RIGHT:	b = SDL_GAMEPAD_BUTTON_EAST;			break;
		case K_GP_DIAMOND_LEFT:		b = SDL_GAMEPAD_BUTTON_WEST;			break;
		case K_GP_DIAMOND_UP:		b = SDL_GAMEPAD_BUTTON_NORTH;			break;
		case K_GP_BACK:				b = SDL_GAMEPAD_BUTTON_BACK;			break;
		case K_GP_GUIDE:			b = SDL_GAMEPAD_BUTTON_GUIDE;			break;
		case K_GP_START:			b = SDL_GAMEPAD_BUTTON_START;			break;
		case K_GP_LEFT_STICK:		b = SDL_GAMEPAD_BUTTON_LEFT_STICK;		break;
		case K_GP_RIGHT_STICK:		b = SDL_GAMEPAD_BUTTON_RIGHT_STICK;		break;
		case K_GP_LEFT_SHOULDER:	b = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;	break;
		case K_GP_RIGHT_SHOULDER:	b = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;	break;
		case K_GP_DPAD_UP:			b = SDL_GAMEPAD_BUTTON_DPAD_UP;			break;
		case K_GP_DPAD_DOWN:		b = SDL_GAMEPAD_BUTTON_DPAD_DOWN;		break;
		case K_GP_DPAD_LEFT:		b = SDL_GAMEPAD_BUTTON_DPAD_LEFT;		break;
		case K_GP_DPAD_RIGHT:		b = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;		break;
		case K_GP_MISC1:			b = SDL_GAMEPAD_BUTTON_MISC1;			break;
		case K_GP_PADDLE1:			b = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1;	break;
		case K_GP_PADDLE2:			b = SDL_GAMEPAD_BUTTON_LEFT_PADDLE1;	break;
		case K_GP_PADDLE3:			b = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2;	break;
		case K_GP_PADDLE4:			b = SDL_GAMEPAD_BUTTON_LEFT_PADDLE2;	break;
		case K_GP_TOUCHPAD:			b = SDL_GAMEPAD_BUTTON_TOUCHPAD;		break;
		default:
			return false;
		}
		keyname = SDL_GetGamepadStringForButton(b);
	}
	else
		return false;

	if (keyname)
	{
		Q_strncpyz(buf, keyname, bufsize);
		return true;
	}
	return false;
}

static unsigned int tbl_sdltoquakemouse[] =
{
	K_MOUSE1,
	K_MOUSE3,
	K_MOUSE2,
	K_MOUSE4,
	K_MOUSE5,
	K_MOUSE6,
	K_MOUSE7,
	K_MOUSE8,
	K_MOUSE9,
	K_MOUSE10
};


void INS_SetOSK(int osk)
{
	if (!sdlwindow) return;
	if (osk && sdlwindow)
	{
		SDL_Rect rect;
		int width, height;
		SDL_GetWindowSize(sdlwindow, &width, &height);
		rect.x = 0;
		rect.y = height/2;
		rect.w = width;
		rect.h = height - rect.y;
		SDL_SetTextInputArea(sdlwindow, &rect, 0);

		if (!SDL_TextInputActive(sdlwindow))
		{
			SDL_StartTextInput(sdlwindow);
//			Con_Printf("OSK shown...\n");
		}
	}
	else
	{
		if (SDL_TextInputActive(sdlwindow))
		{
			SDL_StopTextInput(sdlwindow);
//			Con_Printf("OSK shown... killed\n");
		}
	}
}

static void INS_MouseAddRem(SDL_MouseID mid, qboolean added)
{
	if (added)
		return;	//let INS_MouseID add it instead. so we're not adding devices that won't be used at low indexes
	SDL_GiveFinger(-2, mid, -1, !added);
}
static int INS_MouseID(SDL_MouseID mid)
{
	return SDL_GiveFinger(-2, mid, -1, false);
}
SDL_AppResult SDLCALL SDL_AppEvent(void *appstate, SDL_Event *event)
{
	int which;
	switch(event->type)
	{
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		if (qrenderer == QR_SDLGPU)
		{
			vid.pixelwidth = event->window.data1;
			vid.pixelheight = event->window.data2;
			// Preserve the current logical window size for display-mode rollback.
			// Fullscreen must retain the separate size used when returning to a window.
			if (!vid_isfullscreen)
				SDL_GetWindowSize(sdlwindow, &currentrendererstate.width, &currentrendererstate.height);
			{
				extern cvar_t vid_conautoscale, vid_conwidth;	//make sure the screen is updated properly.
				Cvar_ForceCallback(&vid_conautoscale);
				Cvar_ForceCallback(&vid_conwidth);
			}
		}
		break;
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		vid.activeapp = true;
		break;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		vid.activeapp = false;
        Key_ClearStates();
		IN_DeactivateMouse();
		break;
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		Cbuf_AddText("quit prompt\n", RESTRICT_LOCAL);
		break;

	case SDL_EVENT_KEY_UP:
	case SDL_EVENT_KEY_DOWN:
		{
			int s = event->key.scancode;
			int qs = MySDL_MapScancode(s);

			IN_KeyEvent(0, event->key.down, qs, 0);
		}
		break;
/*	case SDL_EVENT_TEXT_EDITING:
		{
			event.edit;
		}
		break;*/
	case SDL_EVENT_TEXT_INPUT:
		{
			unsigned int uc;
			int err;
			const char *text = event->text.text;
			while(*text)
			{
				uc = utf8_decode(&err, text, &text);
				if (uc && !err)
				{
					IN_KeyEvent(0, true, 0, uc);
					IN_KeyEvent(0, false, 0, uc);
				}
			}
		}
		break;

	case SDL_EVENT_FINGER_DOWN:
	case SDL_EVENT_FINGER_UP:
		{
			uint32_t thefinger = SDL_GiveFinger(-1, event->tfinger.touchID, event->tfinger.fingerID, event->type==SDL_EVENT_FINGER_UP);
			IN_MouseMove(thefinger, true, event->tfinger.x * vid.pixelwidth, event->tfinger.y * vid.pixelheight, 0, event->tfinger.pressure);
			IN_KeyEvent(thefinger, event->type==SDL_EVENT_FINGER_DOWN, K_TOUCH, 0);
		}
		break;
	case SDL_EVENT_FINGER_MOTION:
		{
			uint32_t thefinger = SDL_GiveFinger(-1, event->tfinger.touchID, event->tfinger.fingerID, false);
			IN_MouseMove(thefinger, true, event->tfinger.x * vid.pixelwidth, event->tfinger.y * vid.pixelheight, 0, event->tfinger.pressure);
		}
		break;

	case SDL_EVENT_DROP_FILE:
		Host_RunFile(event->drop.data, strlen(event->drop.data), NULL);
		break;

	case SDL_EVENT_MOUSE_MOTION:
		if (event->motion.which == SDL_TOUCH_MOUSEID)
			break;	//ignore legacy touch events.
		which = INS_MouseID(event->motion.which);
        if (!mouseactive) {
            int w, h;
            SDL_GetWindowSize(sdlwindow, &w, &h);
            if (w > 0 && h > 0)
                IN_MouseMove(which, true, event->motion.x * vid.pixelwidth / w, event->motion.y * vid.pixelheight / h, 0, 0);
        }
		else
			IN_MouseMove(which, false, event->motion.xrel, event->motion.yrel, 0, 0);
		break;

	case SDL_EVENT_MOUSE_WHEEL:
		if (event->wheel.which == SDL_TOUCH_MOUSEID)
			break;	//ignore legacy touch events.
		which = INS_MouseID(event->wheel.which);
		if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
			event->wheel.y *= -1;
		for (; event->wheel.y > 0; event->wheel.y--)
		{
			IN_KeyEvent(which, true, K_MWHEELUP, 0);
			IN_KeyEvent(which, false, K_MWHEELUP, 0);
		}
		for (; event->wheel.y < 0; event->wheel.y++)
		{
			IN_KeyEvent(which, true, K_MWHEELDOWN, 0);
			IN_KeyEvent(which, false, K_MWHEELDOWN, 0);
		}
		for (; event->wheel.x > 0; event->wheel.x--)
		{
			IN_KeyEvent(which, true, K_MWHEELRIGHT, 0);
			IN_KeyEvent(which, false, K_MWHEELRIGHT, 0);
		}
		for (; event->wheel.x < 0; event->wheel.x++)
		{
			IN_KeyEvent(which, true, K_MWHEELLEFT, 0);
			IN_KeyEvent(which, false, K_MWHEELLEFT, 0);
		}
		break;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (event->button.which == SDL_TOUCH_MOUSEID)
			break;	//ignore legacy touch events. SDL_FINGER* events above will handle it (for multitouch)
		which = INS_MouseID(event->button.which);
		//Hmm. SDL allows for 255 buttons, but only defines 5...
		if (event->button.button > sizeof(tbl_sdltoquakemouse)/sizeof(tbl_sdltoquakemouse[0]))
			event->button.button = sizeof(tbl_sdltoquakemouse)/sizeof(tbl_sdltoquakemouse[0]);
		IN_KeyEvent(which, event->button.down, tbl_sdltoquakemouse[event->button.button-1], 0);
		break;

	case SDL_EVENT_MOUSE_ADDED:
		INS_MouseAddRem(event->mdevice.which, true);
		break;
	case SDL_EVENT_MOUSE_REMOVED:
		INS_MouseAddRem(event->mdevice.which, false);
		break;

	case SDL_EVENT_TERMINATING:
		Cbuf_AddText("quit force\n", RESTRICT_LOCAL);
		return SDL_APP_SUCCESS;
	case SDL_EVENT_QUIT:
		Cbuf_AddText("quit\n", RESTRICT_LOCAL);
		return SDL_APP_SUCCESS;

	//actually, joysticks *should* work with sdl1 as well, but there are some differences (like no hot plugging, I think).
	case SDL_EVENT_JOYSTICK_AXIS_MOTION:
		J_JoystickAxis(event->jaxis.which, event->jaxis.axis, event->jaxis.value);
		break;
//	case SDL_EVENT_JOYSTICK_BALL_MOTION:
//	case SDL_EVENT_JOYSTICK_HAT_MOTION:
//		break;
	case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
	case SDL_EVENT_JOYSTICK_BUTTON_UP:
		J_JoystickButton(event->jbutton.which, event->jbutton.button, event->type==SDL_EVENT_JOYSTICK_BUTTON_DOWN);
		break;
	case SDL_EVENT_JOYSTICK_ADDED:
		J_JoystickAdded(event->jdevice.which);
		break;
	case SDL_EVENT_JOYSTICK_REMOVED:
		J_Kill(event->jdevice.which, true);
		break;

	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		J_ControllerAxis(event->gaxis.which, event->gaxis.axis, event->gaxis.value);
		break;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
		J_ControllerButton(event->gbutton.which, event->gbutton.button, event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN);
		break;
	case SDL_EVENT_GAMEPAD_ADDED:
		J_ControllerAdded(event->gdevice.which);
		break;
	case SDL_EVENT_GAMEPAD_REMOVED:
		J_Kill(event->gdevice.which, true);
		break;
//	case SDL_EVENT_GAMEPAD_REMAPPED:
//		break;
	case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
		J_ControllerTouchPad(event->gtouchpad.which, event->gtouchpad.touchpad, event->gtouchpad.finger, 1, event->gtouchpad.x, event->gtouchpad.y, event->gtouchpad.pressure);
		break;
	case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
		J_ControllerTouchPad(event->gtouchpad.which, event->gtouchpad.touchpad, event->gtouchpad.finger, 0, event->gtouchpad.x, event->gtouchpad.y, event->gtouchpad.pressure);
		break;
	case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
		J_ControllerTouchPad(event->gtouchpad.which, event->gtouchpad.touchpad, event->gtouchpad.finger, -1, event->gtouchpad.x, event->gtouchpad.y, event->gtouchpad.pressure);
		break;
	case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
		J_ControllerSensor(event->gsensor.which, event->gsensor.sensor, event->gsensor.data);
		break;
	}

	return SDL_APP_CONTINUE;
}


void Sys_SendKeyEvents(void)
{
	SDL_Event event;
	int axis, j;

#ifdef HAVE_SERVER
	if (isDedicated)
	{
		SV_GetConsoleCommands ();
		return;
	}
#endif

	{
		qboolean osk = !!Key_Dest_Has(kdm_console|kdm_cwindows|kdm_message);
		if (Key_Dest_Has(kdm_prompt|kdm_menu))
		{
			j = Menu_WantOSK();
			if (j < 0)
				osk |= sys_osk.ival;
			else
				osk |= j;
		}
		else if (Key_Dest_Has(kdm_game))
			osk |= sys_osk.ival;
		INS_SetOSK(osk);
	}

	while(SDL_PollEvent(&event))
	{
		SDL_AppEvent(NULL, &event);
	}

	for (j = 0; j < MAX_JOYSTICKS; j++)
	{
		struct sdljoy_s *joy = &sdljoy[j];
		if (joy->qdevid == DEVID_UNSET || !joy->controller)
			continue;

		if (joy->buttonheld)
		{
			for (axis = 0; axis < countof(gpbuttonmap); axis++)
			{
				if (joy->buttonheld & (1u<<axis))
				{
					joy->buttonrepeat[axis] -= host_frametime;
					if (joy->buttonrepeat[axis] < 0)
					{
						IN_KeyEvent(joy->qdevid, true, gpbuttonmap[axis], 0);
						joy->buttonrepeat[axis] = 0.25;
					}
				}
			}
		}

		for (axis = 0; axis < countof(gpaxismap); axis++)
		{
			if ((joy->axistobuttonp | joy->axistobuttonn) & (1u<<axis))
			{
				joy->repeattime[axis] -= host_frametime;
				if (joy->repeattime[axis] < 0)
				{
					if (joy->axistobuttonp & (1u<<axis))
					{
						IN_KeyEvent(joy->qdevid, true, gpaxismap[axis].pos, 0);
						joy->repeattime[axis] = 0.25;
					}
					if (joy->axistobuttonn & (1u<<axis))
					{
						IN_KeyEvent(joy->qdevid, true, gpaxismap[axis].neg, 0);
						joy->repeattime[axis] = 0.25;
					}
				}
			}
		}
	}
}






void INS_Shutdown (void)
{
	IN_DeactivateMouse();

	J_KillAll();
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK|SDL_INIT_GAMEPAD);
}

void INS_ReInit (void)
{
	unsigned int i;
	memset(sdljoy, 0, sizeof(sdljoy));
	for (i = 0; i < MAX_JOYSTICKS; i++)
		sdljoy[i].qdevid = DEVID_UNSET;
	SDL_InitSubSystem(SDL_INIT_JOYSTICK|SDL_INIT_GAMEPAD);

	IN_ActivateMouse();

	SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);
}

//stubs, all the work is done in Sys_SendKeyEvents
void INS_Move(void)
{
}
void INS_Init (void)
{
	Cvar_Register(&sys_osk, "input controls");
	Cvar_Register (&joy_only, "input controls");

}
void INS_Accumulate(void)	//input polling
{
}
void INS_Commands (void)	//used to Cbuf_AddText joystick button events in windows.
{
}
void INS_EnumerateDevices(void *ctx, void(*callback)(void *ctx, const char *type, const char *devicename, unsigned int *qdevid))
{
	unsigned int i;
	for (i = 0; i < MAX_JOYSTICKS; i++)
		if (sdljoy[i].controller || sdljoy[i].joystick)
			callback(ctx, "joy", sdljoy[i].devname, &sdljoy[i].qdevid);

	//pointer devices...
	for (i = 0; i < MAX_FINGERS; i++)
	{
		unsigned int d = i;
		if (!sdlfinger[i].active)
			continue;
		else if (sdlfinger[i].jid == -2)
			callback(ctx, "mouse", SDL_GetMouseNameForID(sdlfinger[i].tid), &d);
		else if (sdlfinger[i].jid == -1)
			callback(ctx, "finger", SDL_GetTouchDeviceName(sdlfinger[i].tid), &d);
		else
			callback(ctx, "joypad", SDL_GetGamepadNameForID(sdlfinger[i].jid), &d);
	}

	//sdl3 allows tracking keyboards. too lazy to deal with that.
}


