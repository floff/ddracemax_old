/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <engine/e_config.h>
#include <engine/e_server_interface.h>
#include <game/generated/g_protocol.hpp>
#include <game/server/gamecontext.hpp>
#include "light.hpp"

//////////////////////////////////////////////////
// LIGHT
//////////////////////////////////////////////////

const int TICK=(server_tickspeed()*0.15f);

LIGHT::LIGHT(vec2 pos, float rotation, int length)
: ENTITY(NETOBJTYPE_LASER)
{
	this->pos = pos;
	this->rotation = rotation;
	this->length = length;
	this->eval_tick=server_tick();
	game.world.insert_entity(this);
	step();
}


bool LIGHT::hit_character()
{
	vec2 nothing;
	CHARACTER *hit = game.world.intersect_character(pos, to, 0.0f, nothing, 0);
	if(!hit)
		return false;
	hit->freeze(server_tickspeed()*3);
	return true;

}

void LIGHT::move()
{
	if (speed != 0)
	{
		if ((cur_length>=length && speed>0) || (cur_length<=0 && speed<0))
				speed=-speed;
		cur_length+=speed*TICK + length_l;
		length_l=0;
		if (cur_length>length)
		{
			length_l=cur_length-length;
			cur_length=length;
		}
		else if(cur_length<0)
		{
			length_l=0+cur_length;
			cur_length=0;
		}
	}
	
	rotation+=ang_speed*TICK;
	if (rotation>pi*2)
		rotation-=pi*2;
	else if(rotation<0)
		rotation+=pi*2;
}

void LIGHT::step()
{
	move();
	vec2 dir;
	dir.x=sin(rotation);
	dir.y=cos(rotation);
	vec2 to2 = pos + normalize(dir)*cur_length;
	col_intersect_nolaser(pos, to2, &to,0 );
}
	
void LIGHT::reset()
{
	//game.world.destroy_entity(this);
}

void LIGHT::tick()
{
	
	if (server_tick() > eval_tick+TICK)
	{
		eval_tick=server_tick();
		step();
	}
	hit_character();
	return;


}

void LIGHT::snap(int snapping_client)
{
	if(networkclipped(snapping_client,pos) && networkclipped(snapping_client,to))
		return;

	
	NETOBJ_LASER *obj = (NETOBJ_LASER *)snap_new_item(NETOBJTYPE_LASER, id, sizeof(NETOBJ_LASER));
	obj->x = (int)pos.x;
	obj->y = (int)pos.y;
	obj->from_x = (int)to.x;
	obj->from_y = (int)to.y;


	int start_tick = eval_tick;
	if (start_tick<server_tick()-4)
		start_tick=server_tick()-4;
	else if (start_tick>server_tick())
		start_tick=server_tick();
	obj->start_tick = start_tick;
}
