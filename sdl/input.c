/*
 * input.c
 * Copyright (C) 1998 Brainchild Design - http://brainchilddesign.com/
 * 
 * Copyright (C) 2001 Chuck Mason <cemason@users.sourceforge.net>
 *
 * Copyright (C) 2002 Florian Schulze <crow@icculus.org>
 *
 * This file is part of Jump'n'Bump.
 *
 * Jump'n'Bump is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Jump'n'Bump is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "globals.h"

#include <string.h>

#include <SDL_platform.h>

int num_joys=0;
SDL_Joystick* joys[4];
SDL_Joystick* accelerometer = NULL;

/* assumes joysticks have at least one button, could check numbuttons first? */
#ifdef __IPHONEOS__
#	define ACCELEROMETER_JOY_SUPPORTED
#	define ACCELEROMETER_JOY_LEFT() (accelerometer && SDL_JoystickGetAxis(accelerometer, 1)>350)
#	define ACCELEROMETER_JOY_RIGHT() (accelerometer && SDL_JoystickGetAxis(accelerometer, 1)<-350)
#elif defined(__ANDROID__)
#	define ACCELEROMETER_JOY_SUPPORTED
#	define ACCELEROMETER_JOY_LEFT() (accelerometer && SDL_JoystickGetAxis(accelerometer, 0)<-1500)
#	define ACCELEROMETER_JOY_RIGHT() (accelerometer && SDL_JoystickGetAxis(accelerometer, 0)>1500)
#else
#	define ACCELEROMETER_JOY_LEFT() (0)
#	define ACCELEROMETER_JOY_RIGHT() (0)
#endif

#ifdef __SWITCH__
extern int single_joycon_mode;
extern int keep_aspect;
static int can_change_single_joycon_mode = 1;
static int can_change_keep_aspect = 1;
#define JOY_CHANGESINGLEJOYCON(num) (num_joys>num && SDL_JoystickGetButton(joys[num], 6))
#define JOY_CHANGEKEEPASPECT(num) (num_joys>num && SDL_JoystickGetButton(joys[num], 7))
#define JOY_JUMP(num) (num_joys>num && (SDL_JoystickGetButton(joys[num], 0) || SDL_JoystickGetButton(joys[num], 1) || SDL_JoystickGetButton(joys[num], 2) || SDL_JoystickGetButton(joys[num], 3)))
#define JOY_LEFT(num) (num_joys>num && (SDL_JoystickGetButton(joys[num], 16) || SDL_JoystickGetButton(joys[num], 12)))
#define JOY_RIGHT(num) (num_joys>num && (SDL_JoystickGetButton(joys[num], 18) || SDL_JoystickGetButton(joys[num], 14)))
#elif defined(__PSP2__)
extern int keep_aspect;
static int can_change_keep_aspect = 1;
#define JOY_CHANGEKEEPASPECT(num) (num_joys>num && (SDL_JoystickGetButton(joys[num], 4) || SDL_JoystickGetButton(joys[num], 5)))
#define JOY_JUMP(num) (num_joys>num && (SDL_JoystickGetButton(joys[num], 0) || SDL_JoystickGetButton(joys[num], 1) || SDL_JoystickGetButton(joys[num], 2) || SDL_JoystickGetButton(joys[num], 3)))
#define JOY_LEFT(num) (num_joys>num && ((SDL_JoystickGetAxis(joys[num], 0)<-3200) || SDL_JoystickGetButton(joys[num], 7)))
#define JOY_RIGHT(num) (num_joys>num && ((SDL_JoystickGetAxis(joys[num], 0)>3200) || SDL_JoystickGetButton(joys[num], 9)))
#else
/* I find using the vertical axis to be annoying -- dnb */
#define JOY_JUMP(num) (num_joys>num && (SDL_JoystickGetButton(joys[num], 0) || SDL_JoystickGetButton(joys[num], 20)))
#define JOY_LEFT(num) (num_joys>num && SDL_JoystickGetAxis(joys[num], 0)<-3200)
#define JOY_RIGHT(num) (num_joys>num && SDL_JoystickGetAxis(joys[num], 0)>3200)
#endif

void update_player_actions(void)
{
	int tmp;

	if (client_player_num < 0) {
		tmp = (key_pressed(KEY_PL1_LEFT) == 1) || ACCELEROMETER_JOY_LEFT() || JOY_LEFT(3);
		if (tmp != player[0].action_left)
			tellServerPlayerMoved(0, MOVEMENT_LEFT, tmp);
		tmp = (key_pressed(KEY_PL1_RIGHT) == 1) || ACCELEROMETER_JOY_RIGHT() || JOY_RIGHT(3);
		if (tmp != player[0].action_right)
			tellServerPlayerMoved(0, MOVEMENT_RIGHT, tmp);
		tmp = (key_pressed(KEY_PL1_JUMP) == 1) || JOY_JUMP(3);
		if (tmp != player[0].action_up)
			tellServerPlayerMoved(0, MOVEMENT_UP, tmp);

		tmp = (key_pressed(KEY_PL2_LEFT) == 1) || JOY_LEFT(2);
		if (tmp != player[1].action_left)
			tellServerPlayerMoved(1, MOVEMENT_LEFT, tmp);
		tmp = (key_pressed(KEY_PL2_RIGHT) == 1) || JOY_RIGHT(2);
		if (tmp != player[1].action_right)
			tellServerPlayerMoved(1, MOVEMENT_RIGHT, tmp);
		tmp = (key_pressed(KEY_PL2_JUMP) == 1) || JOY_JUMP(2);
		if (tmp != player[1].action_up)
			tellServerPlayerMoved(1, MOVEMENT_UP, tmp);

		tmp = (key_pressed(KEY_PL3_LEFT) == 1) || JOY_LEFT(1);
		if (tmp != player[2].action_left)
			tellServerPlayerMoved(2, MOVEMENT_LEFT, tmp);
		tmp = (key_pressed(KEY_PL3_RIGHT) == 1) || JOY_RIGHT(1);
		if (tmp != player[2].action_right)
			tellServerPlayerMoved(2, MOVEMENT_RIGHT, tmp);
		tmp = (key_pressed(KEY_PL3_JUMP) == 1) || JOY_JUMP(1);
		if (tmp != player[2].action_up)
			tellServerPlayerMoved(2, MOVEMENT_UP, tmp);

		tmp = (key_pressed(KEY_PL4_LEFT) == 1) || JOY_LEFT(0);
		if (tmp != player[3].action_left)
		tellServerPlayerMoved(3, MOVEMENT_LEFT, tmp);
		tmp = (key_pressed(KEY_PL4_RIGHT) == 1) || JOY_RIGHT(0);
		if (tmp != player[3].action_right)
		tellServerPlayerMoved(3, MOVEMENT_RIGHT, tmp);
		tmp = (key_pressed(KEY_PL4_JUMP) == 1) || JOY_JUMP(0);
		if (tmp != player[3].action_up)
		tellServerPlayerMoved(3, MOVEMENT_UP, tmp);
	} else {
		tmp = (key_pressed(KEY_PL1_LEFT) == 1) || JOY_LEFT(0);
		if (tmp != player[client_player_num].action_left)
			tellServerPlayerMoved(client_player_num, MOVEMENT_LEFT, tmp);
		tmp = (key_pressed(KEY_PL1_RIGHT) == 1) || JOY_RIGHT(0);
		if (tmp != player[client_player_num].action_right)
			tellServerPlayerMoved(client_player_num, MOVEMENT_RIGHT, tmp);
		tmp = (key_pressed(KEY_PL1_JUMP) == 1) || JOY_JUMP(0);
		if (tmp != player[client_player_num].action_up)
			tellServerPlayerMoved(client_player_num, MOVEMENT_UP, tmp);
	}
#if defined(__SWITCH__) || defined(__PSP2__)
	if (can_change_keep_aspect && JOY_CHANGEKEEPASPECT(0)) {
		can_change_keep_aspect = 0;
		keep_aspect++;
		keep_aspect %= 3;
	} 
	if (!can_change_keep_aspect && !JOY_CHANGEKEEPASPECT(0)) {
		can_change_keep_aspect = 1;
	}
#endif

#ifdef __SWITCH__
	if (can_change_single_joycon_mode && JOY_CHANGESINGLEJOYCON(0)) {
		can_change_single_joycon_mode = 0;
		single_joycon_mode = !single_joycon_mode;
	} 
	if (!can_change_single_joycon_mode && !JOY_CHANGESINGLEJOYCON(0)) {
		can_change_single_joycon_mode = 1;
	}
#endif
}

void init_inputs(void)
{
	int i, j = 0;

	num_joys = SDL_NumJoysticks();
	for(i = 0; i < num_joys; ++i) {
		int is_accelerometer = 0;
#ifdef ACCELEROMETER_JOY_SUPPORTED
		if (!accelerometer) {
			const char* name = SDL_JoystickNameForIndex(i);
			if (name && strcasestr(name, "accelerometer")) {
				is_accelerometer = 1;
			}
		}
#endif

		if ((is_accelerometer) || (j < 4))
		{
			SDL_Joystick* joystick = SDL_JoystickOpen(i);
			if (joystick) {
				if (is_accelerometer) {
					accelerometer = joystick;
				} else {
					joys[j] = joystick;
					++j;
				}
			} else {
				fprintf(stderr, "Could not open joystick %d", i);
			}
		}
	}

	main_info.mouse_enabled = 0;
	main_info.joy_enabled = 0;
}
