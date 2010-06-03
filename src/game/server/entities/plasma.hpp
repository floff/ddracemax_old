/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */

#ifndef PLASMA_TYPE
#define PLASMA_TYPE

#include <game/server/entity.hpp>

class GUN;

class PLASMA : public ENTITY
{
	vec2 core;
	int eval_tick;
	int lifetime;

	bool hit_character();
	void move();
public:
	PLASMA(vec2 pos,vec2 dir);

	virtual void reset();
	virtual void tick();
	virtual void snap(int snapping_client);
};


#endif