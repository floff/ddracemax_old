/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <engine/e_config.h>
#include <engine/e_server_interface.h>
#include <game/generated/g_protocol.hpp>
#include <game/server/gamecontext.hpp>
#include "drager.hpp"

//////////////////////////////////////////////////
// DRAGER
//////////////////////////////////////////////////

const int TICK=(server_tickspeed()*0.30f);
const int LENGTH=700;

DRAGER::DRAGER(vec2 pos, float strength)
: ENTITY(NETOBJTYPE_LASER)
{
	this->pos = pos;
	this->strength = strength;
	this->eval_tick=server_tick();
	game.world.insert_entity(this);
}

void DRAGER::move()
{
	if (target)
		return;
	CHARACTER *ents[16];
	int num = -1;
	num = game.world.find_entities(pos,LENGTH, (ENTITY**)ents, 16, NETOBJTYPE_CHARACTER);
	int id=-1;
	int minlen=0;
	for (int i = 0; i < num; i++)
	{
		target = ents[i];
		int res;
		res = col_intersect_line(pos, target->pos, 0, 0);
		if (!res)
		{
			int len=length(ents[i]->pos - pos);
			if (minlen==0 || minlen>len)
			{
				minlen=len;
				id=i;
			}
		}
	}
	if (id!=-1)
	{
		target = ents[id];
	}
	else
	{
		target=0;
	}
}

void DRAGER::drag()
{
	if (target)
	{
		int res = col_intersect_line(pos, target->pos, 0, 0);
		if (res || length(pos-target->pos)>700)
		{
			target=0;
		}
		else
			if (length(pos-target->pos)>28)
			{
				target->core.vel+=normalize(pos-target->pos)*strength;
			}
	}
}
	
void DRAGER::reset()
{
	//game.world.destroy_entity(this);
}

void DRAGER::tick()
{
	
	if (server_tick() > eval_tick+TICK)
	{
		eval_tick=server_tick();
		move();
	}
	drag();
	return;


}

void DRAGER::snap(int snapping_client)
{
	if (target)
	{
		if(networkclipped(snapping_client,pos) && networkclipped(snapping_client,target->pos))
			return;
	}
	else
		if(networkclipped(snapping_client,pos))
			return;

	NETOBJ_LASER *obj = (NETOBJ_LASER *)snap_new_item(NETOBJTYPE_LASER, id, sizeof(NETOBJ_LASER));

	obj->x = (int)pos.x;
	obj->y = (int)pos.y;	
	if (target)
	{
		obj->from_x = (int)target->pos.x;
		obj->from_y = (int)target->pos.y;
	}
	else
	{
		obj->from_x = (int)pos.x;
		obj->from_y = (int)pos.y;
	}
	

	int start_tick = eval_tick;
	if (start_tick<server_tick()-4)
		start_tick=server_tick()-4;
	else if (start_tick>server_tick())
		start_tick=server_tick();
	obj->start_tick = start_tick;
}
//я тут был