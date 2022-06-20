
///////////////////////////////////////////////////////////////////////////////
// Headers.

#include <stdint.h>
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "glob.hpp"
#include "UART.hpp"
#include <iostream>
#include <memory>
#include "sprites_rgb333.h"
#include <pthread.h>

///////////////////////////////////////////////////////////////////////////////
// HW stuff.

#define WAIT_UNITL_0(x) while(x != 0){}
#define WAIT_UNITL_1(x) while(x != 1){}

#define SCREEN_IDX1_W 640
#define SCREEN_IDX1_H 480
#define SCREEN_IDX4_W 320
#define SCREEN_IDX4_H 240
#define SCREEN_RGB333_W 160
#define SCREEN_RGB333_H 120

#define SCREEN_IDX4_W8 (SCREEN_IDX4_W/8)

#define gpu_p32 ((volatile uint32_t*)LPRS2_GPU_BASE)
#define palette_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x1000))
#define unpack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x400000))
#define pack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x600000))
#define unpack_idx4_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x800000))
#define pack_idx4_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0xa00000))
#define unpack_rgb333_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0xc00000))
#define joypad_p32 ((volatile uint32_t*)LPRS2_JOYPAD_BASE)

#define BAUDRATE 1200
#define UART_DEV_PATTERN "/dev/ttyUSB*"

using namespace std;

typedef struct {
	unsigned a      : 1;
	unsigned b      : 1;
	unsigned z      : 1;
	unsigned start  : 1;
	unsigned up     : 1;
	unsigned down   : 1;
	unsigned left   : 1;
	unsigned right  : 1;
} bf_joypad;
#define joypad (*((volatile bf_joypad*)LPRS2_JOYPAD_BASE))

typedef struct {
	uint32_t m[SCREEN_IDX1_H][SCREEN_IDX1_W];
} bf_unpack_idx1;
#define unpack_idx1 (*((volatile bf_unpack_idx1*)unpack_idx1_p32))



///////////////////////////////////////////////////////////////////////////////
// Game config.

#define STEP 1


#define PACMAN_ANIM_DELAY 3


///////////////////////////////////////////////////////////////////////////////
// Game data structures.

typedef struct {
	uint16_t x;
	uint16_t y;
} point_t;



typedef enum {
	PACMAN_IDLE,
	PACMAN_OPENING_MOUTH,
	PACMAN_WITH_OPEN_MOUTH,
	PACMAN_CLOSING_MOUTH,
	PACMAN_WITH_CLOSED_MOUTH,
	PACMAN_DEAD
} pacman_anim_states_t;

typedef enum {
	GHOST_LEFT,
	GHOST_RIGHT,
	GHOST_DOWN,
	GHOST_UP,
	GHOST_IDLE,
	GHOST_END
} ghost_anim_states_t;



typedef struct {
	pacman_anim_states_t state;
	uint8_t delay_cnt;
} pacman_anim_t;

typedef struct {
	ghost_anim_states_t state_g;
	uint8_t delay_cnt_g;
} ghost_anim_t;

typedef struct {
	point_t pos;
	point_t endpoint;
	pacman_anim_t anim;
} pacman_t;

typedef struct {
	point_t pos;
	ghost_anim_t anim;
} ghost_t;




typedef struct {
	pacman_t pacman;
	ghost_t ghost;
} game_state_t;

typedef struct {
	point_t pos;
	point_t endpoint;
} food_t;




void draw_sprite_from_atlas(
	uint16_t src_x,
	uint16_t src_y,
	uint16_t w,
	uint16_t h,
	uint16_t dst_x,
	uint16_t dst_y
) {
	
	
	for(uint16_t y = 0; y < h; y++){
		for(uint16_t x = 0; x < w; x++){
			uint32_t src_idx = 
				(src_y+y)*Pacman_Sprite_Map__w +
				(src_x+x);
			uint32_t dst_idx = 
				(dst_y+y)*SCREEN_RGB333_W +
				(dst_x+x);
			uint16_t pixel = Pacman_Sprite_Map__p[src_idx];
			unpack_rgb333_p32[dst_idx] = pixel;
		}
	}
	
	
}

char arduino_x;
int mov_x = 0;
int mov_y = 0;
int state = 1;
int direction = 1;

game_state_t gs;

void busy_wait(uint32_t ms){
	//TODO	
	for (uint32_t i =0; i<ms*10000000; i++){}
}


void* read_mov(void* params){
    //char arduino_x;
    vector<string> usb_devs = glob(UART_DEV_PATTERN);
	cout << "INFO: USB_Logger: usb_devs:" << endl;
	for(const auto usb_dev: usb_devs){
		cout << "\t" << usb_dev << endl;
	}

	vector<shared_ptr<UART>> serials;
	for(auto usb_dev: usb_devs){
		auto s = new UART(
			usb_dev,
			BAUDRATE
		);
		serials.emplace_back(s);
	}
	if(serials.empty()){
		cout << "WARN: USB_Logger: No any USB UART dev plugged in!" << endl;
		return 0;
	}
	
	shared_ptr<UART> serial = serials[serials.size()-1];
	
    while(1){
		serial->read(arduino_x);
		if(arduino_x == 'd'){
				direction = 1;
			}else if(arduino_x == 'a'){
				direction = 2;
			}else if(arduino_x == 's'){
				direction = 3;
			}else if(arduino_x == 'w'){
				direction = 4;
			}
        
		if(gs.pacman.anim.state != PACMAN_DEAD){
			if(direction == 1){
				mov_x = +1;
				mov_y = 0;
				state = 1;
			}else if(direction == 2){
				mov_x = -1;
				mov_y = 0;
				state = 2;
			}else if(direction == 3){
				mov_y = +1;
				mov_x = 0;
				state = 3;
			}else if(direction == 4){
				mov_y = -1;
				mov_x = 0;
				state = 4;
			}
		}
        if(arduino_x == 'd' && gs.pacman.pos.x == (SCREEN_RGB333_W - 16)){
			mov_x = 0;
			gs.pacman.pos.x += mov_x*STEP;
		}
		else if(arduino_x == 'a' && gs.pacman.pos.x == 0){
			mov_x = 0;
			gs.pacman.pos.x += mov_x*STEP;
		}
		else if(arduino_x == 'w' && gs.pacman.pos.y == 0){
			mov_y = 0;
			gs.pacman.pos.y += mov_y*STEP;
		}
		else if(arduino_x == 's' && gs.pacman.pos.y == (SCREEN_RGB333_H - 16)){
			mov_y = 0;
			gs.pacman.pos.y += mov_y*STEP;
		}
		else{
			gs.pacman.pos.x += mov_x*STEP;
			gs.pacman.pos.y += mov_y*STEP;
		}
		
    }
}

int parno=1;
int game;
///////////////////////////////////////////////////////////////////////////////
// Game code.

int main(void) {
	srand(time(0));
	// Setup.
	gpu_p32[0] = 3; // RGB333 mode.
	gpu_p32[0x800] = 0x00ff00ff; // Magenta for HUD.


	// Game state.
	//game_state_t gs;
	gs.pacman.pos.x = 32;
	gs.pacman.pos.y = 32;
	gs.pacman.anim.state = PACMAN_IDLE;
	gs.pacman.anim.delay_cnt = 0;

	gs.ghost.pos.x = 100;
	gs.ghost.pos.y = 100;
	gs.ghost.anim.state_g = GHOST_IDLE;
	gs.ghost.anim.delay_cnt_g = 4;

	food_t food;
	food.pos.x = rand() % (SCREEN_RGB333_W - 8);
	food.pos.y = rand() % (SCREEN_RGB333_H - 12);
	
	
	int g_mov;
	int d = 0;

    

    int p = 0;

    pthread_t thread;
	pthread_create(&thread, NULL, read_mov, (void*)&p);
    
	while(1){
		bool drawn = false;
		
		/////////////////////////////////////
		// Poll controls.
		
		
        
		
		/////////////////////////////////////
		// Gameplay.
		


		if(gs.ghost.anim.delay_cnt_g == 0){
			gs.ghost.anim.delay_cnt_g = 4;
			if(gs.pacman.pos.x - gs.ghost.pos.x > 0 && gs.ghost.pos.x + STEP + 16 != SCREEN_RGB333_W){
				gs.ghost.pos.x += STEP;
				g_mov = 1;
			}
			else if(gs.pacman.pos.x - gs.ghost.pos.x < 0 && gs.ghost.pos.x != 0){
				gs.ghost.pos.x -= STEP;
				g_mov = 2;
			}
			if(gs.pacman.pos.y - gs.ghost.pos.y > 0 && gs.ghost.pos.y + STEP + 16 != SCREEN_RGB333_H){
				gs.ghost.pos.y += STEP;
				g_mov = 3;
			}
			else if(gs.pacman.pos.y - gs.ghost.pos.y < 0 && gs.ghost.pos.y != 0){
				gs.ghost.pos.y -= STEP;
				g_mov = 4;
			}
		}else{gs.ghost.anim.delay_cnt_g -= 1;}

		switch(gs.pacman.anim.state){
		case PACMAN_IDLE:
			if(mov_x != 0 || mov_y != 0){
				gs.pacman.anim.delay_cnt = PACMAN_ANIM_DELAY;
				gs.pacman.anim.state = PACMAN_WITH_OPEN_MOUTH;
			}
			break;
		case PACMAN_OPENING_MOUTH:
			if(gs.pacman.anim.delay_cnt != 0){
					gs.pacman.anim.delay_cnt--;
			}else{
				gs.pacman.anim.delay_cnt = PACMAN_ANIM_DELAY;
				gs.pacman.anim.state = PACMAN_WITH_OPEN_MOUTH;
			}
			break;
		case PACMAN_WITH_OPEN_MOUTH:
			if(gs.pacman.anim.delay_cnt != 0){
					gs.pacman.anim.delay_cnt--;
			}else{
				if(mov_x != 0 || mov_y != 0){
					gs.pacman.anim.delay_cnt = PACMAN_ANIM_DELAY;
					gs.pacman.anim.state = PACMAN_CLOSING_MOUTH;
				}else{
					gs.pacman.anim.state = PACMAN_IDLE;
				}
			}
			break;
		case PACMAN_CLOSING_MOUTH:
			if(gs.pacman.anim.delay_cnt != 0){
					gs.pacman.anim.delay_cnt--;
			}else{
				gs.pacman.anim.delay_cnt = PACMAN_ANIM_DELAY;
				gs.pacman.anim.state = PACMAN_WITH_CLOSED_MOUTH;
			}
			break;
		case PACMAN_WITH_CLOSED_MOUTH:
			if(gs.pacman.anim.delay_cnt != 0){
					gs.pacman.anim.delay_cnt--;
			}else{
				if(mov_x != 0 || mov_y != 0){
					gs.pacman.anim.delay_cnt = PACMAN_ANIM_DELAY;
					gs.pacman.anim.state = PACMAN_OPENING_MOUTH;
				}else{
					gs.pacman.anim.state = PACMAN_IDLE;
				}
			}
			break;
		case PACMAN_DEAD:
			if(d == 0){
				d = 1;				
				break;
			}else{
				int i = 32;
				while(1){
					if(i > 208)
						while(1){}
					if(gs.pacman.anim.delay_cnt != 0){
						gs.pacman.anim.delay_cnt--;
					}else{
						gs.pacman.anim.delay_cnt = 3;
						draw_sprite_from_atlas(
							i, 0, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y);
						i += 16;
					}
				}
			}
	
			break;
		}
		
		switch(g_mov){
			case 1:
				gs.ghost.anim.state_g = GHOST_RIGHT;
				break;
			case 2:
				gs.ghost.anim.state_g = GHOST_LEFT;
				break;
			case 3:
				gs.ghost.anim.state_g = GHOST_DOWN;
				break;
			case 4:
				gs.ghost.anim.state_g = GHOST_UP;
				break;
				
		}


		if((gs.ghost.pos.x+14>gs.pacman.pos.x)&&(gs.ghost.pos.x<gs.pacman.pos.x+14)&&
			(gs.ghost.pos.y+14>gs.pacman.pos.y)&&(gs.ghost.pos.y<gs.pacman.pos.y+14))
		{
			gs.ghost.anim.state_g = GHOST_END;
			gs.pacman.anim.state = PACMAN_DEAD;
		}

		if((food.pos.x+8>gs.pacman.pos.x)&&(food.pos.x<gs.pacman.pos.x+8)&&
			(food.pos.y+12>gs.pacman.pos.y)&&(food.pos.y<gs.pacman.pos.y+12))
		{				
			food.pos.x = rand() % (SCREEN_RGB333_W - 8);
			food.pos.y = rand() % (SCREEN_RGB333_H - 12);
			if(!drawn){
				draw_sprite_from_atlas(
						4, 192, 8, 12, food.pos.x, food.pos.y
					);
					drawn = true;
			}
	
		}
		
		/////////////////////////////////////
		// Drawing.
		
		
		// Detecting rising edge of VSync.
		WAIT_UNITL_0(gpu_p32[2]);
		WAIT_UNITL_1(gpu_p32[2]);
		// Draw in buffer while it is in VSync.
		
		
		
		
		// Black background.
		for(uint16_t r = 0; r < SCREEN_RGB333_H; r++){
			for(uint16_t c = 0; c < SCREEN_RGB333_W; c++){
				unpack_rgb333_p32[r*SCREEN_RGB333_W + c] = 0000;
			}
		}
		
		// Draw pacman.
		int a,b;
		switch(gs.pacman.anim.state){
		case PACMAN_IDLE:
			draw_sprite_from_atlas(
					32, 0, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y
				);
			break;
		case PACMAN_OPENING_MOUTH:
		case PACMAN_CLOSING_MOUTH:
			// Half open mouth.
			if(arduino_x == 'd') {
			a=16; b=0;
			}
			draw_sprite_from_atlas(
				a, b, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y
			);
			if(arduino_x == 'a') {
			a=16; b=16;
			}
			draw_sprite_from_atlas(
				a, b, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y
			);
			if(arduino_x == 'w') {
			a=16; b=32;
			}
			draw_sprite_from_atlas(
				a, b, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y
			);
			if(arduino_x == 's') {
			a=16; b=48;
			}
			draw_sprite_from_atlas(
				a, b, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y
			);
			break;
		case PACMAN_WITH_OPEN_MOUTH:
			// Full open mouth.
			if(arduino_x == 'd'){
			a=0; b=0;
			}
			draw_sprite_from_atlas(
				a, b, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y
			);
			if(arduino_x == 'a'){
			a=0; b=16;
			}
			draw_sprite_from_atlas(
				a, b, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y
			);
			if(arduino_x == 'w'){
			a=0; b=32;
			}
			draw_sprite_from_atlas(
				a, b, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y
			);
			if(arduino_x == 's'){
			a=0; b=48;
			}
			draw_sprite_from_atlas(
				a, b, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y
			);
			break;
		case PACMAN_WITH_CLOSED_MOUTH:
			// Close mouth.
			draw_sprite_from_atlas(
				32, 0, 16, 16, gs.pacman.pos.x, gs.pacman.pos.y
			);
			break;
		}
		if(gs.ghost.anim.state_g != GHOST_END){
			switch(gs.ghost.anim.state_g){
				case GHOST_RIGHT:
					if(parno==1){
						draw_sprite_from_atlas(0, 64, 16, 16, gs.ghost.pos.x, gs.ghost.pos.y);
						parno=0;
					}
					else{
						draw_sprite_from_atlas(17, 64, 16, 16, gs.ghost.pos.x, gs.ghost.pos.y);
						parno=1;
					}
					break;
				case GHOST_LEFT:
					if(parno==1){
						draw_sprite_from_atlas(33, 64, 16, 16, gs.ghost.pos.x, gs.ghost.pos.y);
						parno=0;
					}
					else{
						draw_sprite_from_atlas(50, 64, 16, 16, gs.ghost.pos.x, gs.ghost.pos.y);
						parno=1;
					}
					
					
					break;
				case GHOST_UP:
					if(parno==1){
						draw_sprite_from_atlas(65, 64, 16, 16, gs.ghost.pos.x, gs.ghost.pos.y);
						parno=0;
					}
					else{
						draw_sprite_from_atlas(83, 64, 16, 16, gs.ghost.pos.x, gs.ghost.pos.y);						
						parno=1;
					}
					break;
				case GHOST_DOWN:
					if(parno==1){
						draw_sprite_from_atlas(97, 64, 16, 16, gs.ghost.pos.x, gs.ghost.pos.y);
						parno=0;
					}
					else{
						draw_sprite_from_atlas(115, 64, 16, 16, gs.ghost.pos.x, gs.ghost.pos.y);
						parno=1;
					}
					break;
			}
		}
		draw_sprite_from_atlas(
					4, 192, 8, 12, food.pos.x, food.pos.y
			);

		
		if((food.pos.x+16>gs.pacman.pos.x)&&(food.pos.x<gs.pacman.pos.x+16)&&
			(food.pos.y+16>gs.pacman.pos.y)&&(food.pos.y<gs.pacman.pos.y+16))
		{				
			food.pos.x = rand() % (SCREEN_RGB333_W - 8);
			food.pos.y = rand() % (SCREEN_RGB333_H - 12);
			if(!drawn){
				draw_sprite_from_atlas(
						4, 192, 8, 12, food.pos.x, food.pos.y
					);
					drawn = true;
			}
	
		}
		
	}

	return 0;
}