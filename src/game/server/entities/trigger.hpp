/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */

#ifndef GAME_SERVER_ENTITY_TRIGGER_H
#define GAME_SERVER_ENTITY_TRIGGER_H

#include <game/server/entity.hpp>

//class DOOR;

class TRIGGER : public ENTITY
{	
	ENTITY *target;

	bool hit_character();
	void open_door(int tick);

public:

	TRIGGER(vec2 pos, ENTITY *target);
	
	virtual void reset();
	virtual void tick();
	virtual void snap(int snapping_client);
};

#endif
