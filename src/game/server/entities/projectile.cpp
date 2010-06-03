#include <engine/e_server_interface.h>
#include <engine/e_config.h>
#include <game/generated/g_protocol.hpp>
#include <game/server/gamecontext.hpp>
#include "projectile.hpp"
#include "pickup.hpp"


//////////////////////////////////////////////////
// projectile
//////////////////////////////////////////////////
PROJECTILE::PROJECTILE(int type, int owner, vec2 pos, vec2 dir, int span,
	bool freeze, int flags, float force, int sound_impact, int weapon)
: ENTITY(NETOBJTYPE_PROJECTILE)
{
	this->type = type;
	this->pos = pos;
	this->direction = dir;
	this->lifespan = span;
	this->owner = owner;
	this->flags = flags;
	this->force = force;
	this->sound_impact = sound_impact;
	this->weapon = weapon;
	this->start_tick = server_tick();
	this->freeze = freeze;
	game.world.insert_entity(this);
}

void PROJECTILE::reset()
{
	if (lifespan>-2)
		game.world.destroy_entity(this);
}

vec2 PROJECTILE::get_pos(float time)
{
	float curvature = 0;
	float speed = 0;
	if(type == WEAPON_GRENADE)
	{
		curvature = tuning.grenade_curvature;
		speed = tuning.grenade_speed;
	}
	else if(type == WEAPON_SHOTGUN)
	{
		curvature = tuning.shotgun_curvature;
		speed = tuning.shotgun_speed;
	}
	else if(type == WEAPON_GUN)
	{
		curvature = tuning.gun_curvature;
		speed = tuning.gun_speed;
	}
	
	return calc_pos(pos, direction, curvature, speed, time);
}


void PROJECTILE::tick()
{
	
	float pt = (server_tick()-start_tick-1)/(float)server_tickspeed();
	float ct = (server_tick()-start_tick)/(float)server_tickspeed();
	vec2 prevpos = get_pos(pt);
	vec2 curpos = get_pos(ct);
	vec2 speed =  curpos - prevpos;

	if (lifespan>-1)
		lifespan--;
	
	vec2 col_pos;
	vec2 new_pos;
	int collide = col_intersect_line(prevpos, curpos, &col_pos, &new_pos);
	CHARACTER *ownerchar;
	if (owner>=0)
		ownerchar = game.get_player_char(owner);
	CHARACTER *targetchr;
	if (freeze)
		targetchr = game.world.intersect_character(prevpos, col_pos, 1.0f, col_pos, ownerchar);
	else
		targetchr = game.world.intersect_character(prevpos, col_pos, 6.0f, col_pos, ownerchar);
	if(targetchr || collide)
	{
		if((flags & PROJECTILE_FLAGS_EXPLODE) && (!targetchr || (targetchr && !freeze)))
		{
			game.create_explosion(col_pos, owner, weapon, false);
			game.create_sound(col_pos, sound_impact);
		}
		else if(targetchr && freeze)
			targetchr->freeze(server_tickspeed()*3);
		if (collide && bouncing!=0)
		{
			start_tick=server_tick();
			pos=new_pos;
			if (bouncing==1)
				direction.x=-direction.x;
			else if (bouncing==2)
				direction.y=-direction.y;
			pos+=direction;
		}
		else if (weapon==WEAPON_GUN)
		{
			game.create_damageind(curpos, -atan2(direction.x,direction.y), 10);
			game.world.destroy_entity(this);
		}
		else
			if (!freeze)
				game.world.destroy_entity(this);
	}
	if (lifespan == -1)
	{
		game.world.destroy_entity(this);
	}
}

void PROJECTILE::fill_info(NETOBJ_PROJECTILE *proj)
{
	proj->x = (int)pos.x;
	proj->y = (int)pos.y;
	proj->vx = (int)(direction.x*100.0f);
	proj->vy = (int)(direction.y*100.0f);
	proj->start_tick = start_tick;
	proj->type = type;
}

void PROJECTILE::snap(int snapping_client)
{
	float ct = (server_tick()-start_tick)/(float)server_tickspeed();
	
	if(networkclipped(snapping_client, get_pos(ct)))
		return;

	NETOBJ_PROJECTILE *proj = (NETOBJ_PROJECTILE *)snap_new_item(NETOBJTYPE_PROJECTILE, id, sizeof(NETOBJ_PROJECTILE));
	fill_info(proj);
}
