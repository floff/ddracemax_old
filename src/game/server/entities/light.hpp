/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */

#ifndef GAME_SERVER_ENTITY_LIGHT_H
#define GAME_SERVER_ENTITY_LIGHT_H

#include <game/server/entity.hpp>

class LIGHT : public ENTITY
{
	float rotation;
	vec2 to;

	int eval_tick;

	bool hit_character();
	void move();
	void step();
	
public:
	int cur_length;
	int length_l;
	float ang_speed;
	int speed;
	int length;

	LIGHT(vec2 pos, float rotation, int length);
	
	virtual void reset();
	virtual void tick();
	virtual void snap(int snapping_client);
};

#endif
