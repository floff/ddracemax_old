/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */

#ifndef GAME_SERVER_ENTITY_DOOR_H
#define GAME_SERVER_ENTITY_DOOR_H

#include <game/server/entity.hpp>

class TRIGGER;

class DOOR : public ENTITY
{
	vec2 to;

	int eval_tick;
	
	bool opened;
	bool hit_character();

public:

	void open(int tick);
	void close();
	DOOR(vec2 pos, float rotation, int length, bool opened);
	
	virtual void reset();
	virtual void tick();
	virtual void snap(int snapping_client);
};

#endif
