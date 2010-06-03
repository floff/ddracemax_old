/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <engine/e_config.h>
#include <engine/e_server_interface.h>
#include <game/generated/g_protocol.hpp>
#include <game/server/gamecontext.hpp>
#include "trigger.hpp"
#include "door.hpp"

//////////////////////////////////////////////////
// DOOR
//////////////////////////////////////////////////

TRIGGER::TRIGGER(vec2 pos, ENTITY *target)
: ENTITY(NETOBJTYPE_PICKUP)
{
	this->pos = pos;
	this->target = target;

	game.world.insert_entity(this);
}

bool TRIGGER::hit_character()
{
	
	CHARACTER *hit = game.world.closest_character(pos, 20.0f, 0);
	if(!hit)
		return false;
	
	return true;
}

void TRIGGER::open_door(int tick)
{
	DOOR *tar = (DOOR*) target;
	tar->open(tick);
}

void TRIGGER::reset()
{
	return;
}

void TRIGGER::tick()
{
	if (server_tick() % 10 == 0)
	{
		if (hit_character())
			open_door(server_tick());
	}

	return;
}

void TRIGGER::snap(int snapping_client)
{
	return;
/*
	if(networkclipped(snapping_client,pos))
		return;

	
	NETOBJ_PICKUP *up = (NETOBJ_PICKUP *)snap_new_item(NETOBJTYPE_PICKUP, id, sizeof(NETOBJ_PICKUP));
	up->x = (int)pos.x;
	up->y = (int)pos.y;
	up->type = POWERUP_ARMOR; // TODO: two diffrent types? what gives?
	//up->subtype = subtype;
*/
}
