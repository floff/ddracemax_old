/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <engine/e_server_interface.h>
#include <engine/e_config.h>
#include <game/generated/g_protocol.hpp>
#include <game/server/gamecontext.hpp>
#include "gun.hpp"
#include "plasma.hpp"

const int DELAY=server_tickspeed()*0.3f;
const int RANGE=700;

//////////////////////////////////////////////////
// GUN
//////////////////////////////////////////////////
GUN::GUN(vec2 pos)
: ENTITY(NETOBJTYPE_LASER)
{
	this->pos = pos;
	this->eval_tick=server_tick();
	
	game.world.insert_entity(this);
}


void GUN::fire()
{
	CHARACTER *ents[16];
	int num = -1;
	num = game.world.find_entities(pos,RANGE, (ENTITY**)ents, 16, NETOBJTYPE_CHARACTER);
	int id=-1;
	int minlen=0;
	for (int i = 0; i < num; i++)
	{
		CHARACTER *target = ents[i];
		int res;
		vec2 coltile;
		res = col_intersect_line(pos, target->pos,0,0);
		if (!res)
		{
			int len=length(ents[i]->pos - pos);
			if (minlen==0)
			{
				minlen=len;
				id=i;
			}
			else if(minlen>len)
			{
				minlen=len;
				id=i;
			}
		}
	}
	if (id!=-1)
	{
		CHARACTER *target = ents[id];
		vec2 fdir = normalize(target->pos - pos);
		new PLASMA(pos,fdir);
	}
	
}
	
void GUN::reset()
{
	//game.world.destroy_entity(this);
}

void GUN::tick()
{
	if(server_tick() > eval_tick+DELAY)
	{
		eval_tick=server_tick();
		fire();
	}
}

void GUN::snap(int snapping_client)
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
