/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <engine/e_config.h>
#include <engine/e_server_interface.h>
#include <game/generated/g_protocol.hpp>
#include <game/server/gamecontext.hpp>
#include "door.hpp"

//////////////////////////////////////////////////
// DOOR
//////////////////////////////////////////////////

class TRIGGER;

DOOR::DOOR(vec2 pos, float rotation, int length, bool opened)
: ENTITY(NETOBJTYPE_LASER)
{
	this->pos = pos;
	this->opened = opened;

	vec2 dir;
	dir.x=sin(rotation);
	dir.y=cos(rotation);
	vec2 to2 = pos + normalize(dir)*length;
	col_intersect_nolaser(pos, to2, &this->to,0 );
	
	game.world.insert_entity(this);
}

void DOOR::open(int tick)
{
	eval_tick=tick;
	opened=true;
}

void DOOR::close()
{
	opened=false;
}

bool DOOR::hit_character()
{
	vec2 nothing;
	CHARACTER *hit = game.world.intersect_character(pos, to, 1.0f, nothing, 0);
	if(!hit)
		return false;
	hit->doored=true;
	//hit->reset_pos();
	return true;

}

void DOOR::reset()
{
	opened=false;
}

void DOOR::tick()
{
	if (opened==false)
		hit_character();
	else if (eval_tick+10<server_tick())
		close();
	return;
}

void DOOR::snap(int snapping_client)
{
	if(networkclipped(snapping_client,pos) && networkclipped(snapping_client,to))
		return;

	
	NETOBJ_LASER *obj = (NETOBJ_LASER *)snap_new_item(NETOBJTYPE_LASER, id, sizeof(NETOBJ_LASER));
	obj->x = (int)pos.x;
	obj->y = (int)pos.y;
	if (!opened)
	{
		obj->from_x = (int)to.x;
		obj->from_y = (int)to.y;
	}
	else
	{
		obj->from_x = (int)pos.x;
		obj->from_y = (int)pos.y;
	}

	obj->start_tick = server_tick();
}
