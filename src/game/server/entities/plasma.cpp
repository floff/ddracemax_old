/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <engine/e_server_interface.h>
#include <engine/e_config.h>
#include <game/generated/g_protocol.hpp>
#include <game/server/gamecontext.hpp>
#include "plasma.hpp"

const float ACCEL=1.1f;


//////////////////////////////////////////////////
// turret
//////////////////////////////////////////////////
PLASMA::PLASMA(vec2 pos,vec2 dir)
: ENTITY(NETOBJTYPE_LASER)
{
	this->pos = pos;
	this->core = dir;
	this->eval_tick=server_tick();
	this->lifetime=server_tickspeed()*1.5;
	
	game.world.insert_entity(this);
}


bool PLASMA::hit_character()
{
	vec2 to2;
	CHARACTER *hit = game.world.intersect_character(pos, pos+core, 0.0f,to2);
	if(!hit)
		return false;
	hit->freeze(server_tickspeed()*3);
	game.world.destroy_entity(this);
	return true;
}

void PLASMA::move()
{
	pos+=core;
	core*=ACCEL;
}
	
void PLASMA::reset()
{
	game.world.destroy_entity(this);
}

void PLASMA::tick()
{
	if (lifetime==0)
	{
		game.world.destroy_entity(this);
		return;
	}
	move();
	hit_character();
	lifetime--;

	int res = col_intersect_nolaser(pos, pos+core,0, 0);
	if(res)
	{
		game.create_explosion(pos, -1, WEAPON_GRENADE, false);
		game.world.destroy_entity(this);
	}
	
}

void PLASMA::snap(int snapping_client)
{	
	if(networkclipped(snapping_client))
		return;

	NETOBJ_LASER *obj = (NETOBJ_LASER *)snap_new_item(NETOBJTYPE_LASER, id, sizeof(NETOBJ_LASER));
	obj->x = (int)pos.x;
	obj->y = (int)pos.y;
	obj->from_x = (int)pos.x;
	obj->from_y = (int)pos.y;
	obj->start_tick = eval_tick;
}
