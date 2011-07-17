#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#include "common.h"
#include "logic.h"
#include "player.h"
#include "network.h"
#include "level.h"

#define SEND_DELAY 0.1f

static double last_send=0;

static bool has_sent_still = false;
static bool has_sent_no_rot = false;
static bool has_sent_no_fire = true;

static void hndl_collision(int mx, int my, double dt);

void logic(double dt) {
	double s = SPEED * dt;

	vector_t last = vector_t(me->pos);
	float last_a = me->angle;

	int mouse_x,mouse_y;
	SDL_GetMouseState( &mouse_x, &mouse_y);
	mouse.x = mouse_x - (window.w/2.0-me->pos.x);
	mouse.y = mouse_y - (window.h/2.0-me->pos.y);
					

	if(me->dead == 0) {
		me->current_base_texture = TEXTURE_BASE;

		if(keys[SDLK_RCTRL])
			me->free_move = true;
		else 
			me->free_move = false;

		//Calculate angle:
		if(!me->free_move) {
			me->angle = atan2(mouse.y - me->pos.y, mouse.x - me->pos.x);
		}

		if(keys[SDLK_SPACE]) {
			me->accelerate(vector_t(
				cos(me->angle),
				sin(me->angle)
				) * s * DASH_SPEED_FACTOR);
			me->current_base_texture = TEXTURE_DASH;
		}

		if(keys[SDLK_d] || keys[SDLK_e]) {
			me->accelerate(vector_t(
				cos(me->angle+PI/2.0),
				sin(me->angle+PI/2.0)
				) * s);
			me->current_base_texture = TEXTURE_LEFT;
		}

		if(keys[SDLK_a]) {
			me->accelerate(vector_t(
				cos(me->angle+PI*1.5),
				sin(me->angle+PI*1.5)
				) * s);
			me->current_base_texture = TEXTURE_RIGHT;
		}

		if(keys[SDLK_w] || keys[SDLK_COMMA]) {
			me->accelerate(vector_t(
				cos(me->angle),
				sin(me->angle)
				) * s);
			me->current_base_texture = TEXTURE_FWD;
		}


		me->logic(dt);

		//Collision detection:
		//float da=me->angle - last_a;
		//vector_t diff = me->pos - last;
		//Collision detect on the collision points:
		for(int i = 0; i < NUM_COLLISION_POINTS; ++i) {
			int mx, my;
			vector_t v = me->collision_point(i);
			calc_map_index(v, mx, my);
			if(map_value(mx, my)>0) {
				hndl_collision(mx,my, dt);
				break;
			}
		}



		if(me->fire) {
			me->calc_fire(true);
		}


		if(last_send+SEND_DELAY < curtime()) {
			char buffer[1024];

			vector_t delta = me->pos - last;
			float da = (me->angle - last_a)/dt;

			delta.x/=dt;
			delta.y/=dt;

			if(delta.norm() > 1 || me->fire) {
					sprintf(buffer, "omg mov %i %f %f %f %i %f %f %f", me->id, me->pos.x, me->pos.y, me->angle, me->current_base_texture, delta.x, delta.y, da);
				send_msg(buffer);
				has_sent_still = false;
			} else if(!has_sent_still){
				sprintf(buffer, "omg mov %i %f %f %f %i 0 0 %f", me->id, me->pos.x, me->pos.y, me->angle, me->current_base_texture, da);
				send_msg(buffer);
				has_sent_still = true;
			} else if(da > 0.1) {
				has_sent_no_rot = false;
				sprintf(buffer, "omg rot %i %f %f",me->id, me->angle, da); 
				send_msg(buffer);
			} else if(!has_sent_no_rot) {
				has_sent_no_rot = true;
				sprintf(buffer, "omg rot %i %f 0",me->id, me->angle); 
				send_msg(buffer);
			}

			if(me->fire) {
				sprintf(buffer, "omg fir %i", me->id);
				send_msg(buffer);
				has_sent_no_fire = false;
			} else if(!has_sent_no_fire) {
				sprintf(buffer, "omg nof %i", me->id);
				send_msg(buffer);
				has_sent_no_fire = false;
			}
				
			last_send = curtime();
		}
	} else {
		++me->dead;
		if(me->dead > RESPAWN_TIME)
			me->spawn();
	}
	//Update other:
	for(int i=0; i < 4; ++i) {
		if(i != me->id && players[i] != NULL) {
			players[i]->logic(dt);	
		}
	}
}

static void hndl_collision(int mx, int my, double dt) {
	printf("Collision with block (%d, %d)\n", mx, my);
	vector_t block = vector_t(mx*64,my*64);
	/*float bx_min, bx_max;
	float by_min, by_max;
	bx_min= block.x-32;
	bx_max = bx_min + 64;
	by_min = block.y-32;
	by_max = by_min+64;

	float dx, dy;
	dx = abs(block.x-me->pos.x);
	dy = abs(block.y-me->pos.y);*/

	//me->velocity*=0.9;

	vector_t repulse = (block - me->pos).normalized().abs();
	printf("Repulse: (%f, %f)\n", repulse.x, repulse.y);

	#define REPULSE_LIMIT 0.5f

	if(repulse.x-repulse.y > REPULSE_LIMIT) {
		me->velocity.x *= -1.0f;
	} else if(repulse.y - repulse.x > REPULSE_LIMIT) {
		me->velocity.y *= -1.0f;
	} else {
		me->velocity *= -1.0f;
	}
/*
	if(by_min < me->pos.y && me->pos.y < by_max) {
		printf("Mirror y\n");
		me->velocity.y *= -1.0f;
	} else if(bx_min < me->pos.x && me->pos.y < bx_max) {
		printf("Mirror x\n");
		me->velocity.x *= -1.0f;
	} else if( dx < dy) {
		printf("Mirror y-2\n");
		me->velocity.y *= -1.0f;
	} else if( dx > dy) {
		printf("Mirror x-2\n");
		me->velocity.x *= -1.0f;
	} else {
		printf("Mirror both\n");
		me->velocity.x *= -1.0f;
		me->velocity.y *= -1.0f;
	}*/
	me->pos += me->velocity*dt*2.0f;
	me->velocity *= 0.9;
	map[my][mx] = 2;
}

Player * create_player(char * nick, int id) {
	printf("Created player %d\n", id);
	Player * p = new Player(nick, id);
	p->spawn();
	players[id] = p;
	return p;
}

double curtime()  {
	struct timeval ts;
	gettimeofday(&ts, NULL);
	return ts.tv_sec + ts.tv_usec/1000000.0;
}

