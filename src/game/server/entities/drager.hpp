/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */

#ifndef GAME_SERVER_ENTITY_DRAGER_H
#define GAME_SERVER_ENTITY_DRAGER_H

#include <game/server/entity.hpp>

class CHARACTER;

class DRAGER : public ENTITY
{
	float strength;
	int eval_tick;
	void move();
	void drag();
	CHARACTER *target;
	
public:

	DRAGER(vec2 pos, float strength);
	
	virtual void reset();
	virtual void tick();
	virtual void snap(int snapping_client);
};

#endif
