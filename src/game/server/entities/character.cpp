#include <new>
#include "../gamemodes/race.hpp"
#include <engine/e_server_interface.h>
#include <engine/e_config.h>
#include <game/server/gamecontext.hpp>
#include <game/mapitems.hpp>
#include <string.h>

#include "character.hpp"
#include "laser.hpp"
#include "light.hpp"
#include "projectile.hpp"

struct INPUT_COUNT
{
	int presses;
	int releases;
};

static INPUT_COUNT count_input(int prev, int cur)
{
	INPUT_COUNT c = {0,0};
	prev &= INPUT_STATE_MASK;
	cur &= INPUT_STATE_MASK;
	int i = prev;
	while(i != cur)
	{
		i = (i+1)&INPUT_STATE_MASK;
		if(i&1)
			c.presses++;
		else
			c.releases++;
	}

	return c;
}


MACRO_ALLOC_POOL_ID_IMPL(CHARACTER, MAX_CLIENTS)

// player
CHARACTER::CHARACTER()
: ENTITY(NETOBJTYPE_CHARACTER)
{
	proximity_radius = phys_size;
}

void CHARACTER::reset()
{
	destroy();
}

bool CHARACTER::spawn(PLAYER *player, vec2 pos, int team)
{
	player_state = PLAYERSTATE_UNKNOWN;
	emote_stop = -1;
	last_action = -1;
	active_weapon = WEAPON_GUN;
	last_weapon = WEAPON_HAMMER;
	unfreezed_weapon = WEAPON_GUN;
	queued_weapon = -1;

	//clear();
	this->player = player;
	this->pos = pos;
	this->team = team;
	
	core.reset();
	core.world = &game.world.core;
	core.pos = pos;
	game.world.core.characters[player->client_id] = &core;

	reckoning_tick = 0;
	mem_zero(&sendcore, sizeof(sendcore));
	mem_zero(&reckoningcore, sizeof(reckoningcore));
	
	game.world.insert_entity(this);
	alive = true;
	player->force_balanced = false;

	game.controller->on_character_spawn(this);

	return true;
}

void CHARACTER::destroy()
{
	game.world.core.characters[player->client_id] = 0;
	alive = false;
}

void CHARACTER::set_weapon(int w)
{
	if(w == active_weapon)
		return;
		
	last_weapon = active_weapon;
	queued_weapon = -1;
	active_weapon = w;
	if(active_weapon < 0 || active_weapon >= NUM_WEAPONS)
		active_weapon = 0;
	
	game.create_sound(pos, SOUND_WEAPON_SWITCH);
}

bool CHARACTER::is_grounded()
{
	if(col_check_point((int)(pos.x+phys_size/2), (int)(pos.y+phys_size/2+5)))
		return true;
	if(col_check_point((int)(pos.x-phys_size/2), (int)(pos.y+phys_size/2+5)))
		return true;
	return false;
}

int CHARACTER::handle_ninja()
{
	if(active_weapon != WEAPON_NINJA)
		return 0;
	
	vec2 direction = normalize(vec2(latest_input.target_x, latest_input.target_y));

	if ((server_tick() - ninja.activationtick) > (data->weapons.ninja.duration * server_tickspeed() / 1000))
	{
		// time's up, return
		weapons[WEAPON_NINJA].got = false;
		active_weapon = last_weapon;
		if(active_weapon == WEAPON_NINJA)
			active_weapon = WEAPON_GUN;
		set_weapon(active_weapon);
		return 0;
	}
	
	// force ninja weapon
	set_weapon(WEAPON_NINJA);

	ninja.currentmovetime--;

	if (ninja.currentmovetime == 0)
	{
		// reset player velocity
		core.vel *= 0.2f;
		//return MODIFIER_RETURNFLAGS_OVERRIDEWEAPON;
	}

	if (ninja.currentmovetime > 0)
	{
		// Set player velocity
		core.vel = ninja.activationdir * data->weapons.ninja.velocity;
		vec2 oldpos = pos;
		move_box(&core.pos, &core.vel, vec2(phys_size, phys_size), 0.0f);
		// reset velocity so the client doesn't predict stuff
		core.vel = vec2(0.0f,0.0f);
		if ((ninja.currentmovetime % 2) == 0)
		{
			//create_smoke(pos);
		}

		// check if we hit anything along the way
		{
			CHARACTER *ents[64];
			vec2 dir = pos - oldpos;
			float radius = phys_size * 2.0f; //length(dir * 0.5f);
			vec2 center = oldpos + dir * 0.5f;
			int num = game.world.find_entities(center, radius, (ENTITY**)ents, 64, NETOBJTYPE_CHARACTER);

			for (int i = 0; i < num; i++)
			{
				// Check if entity is a player
				if (ents[i] == this)
					continue;
				// make sure we haven't hit this object before
				bool balreadyhit = false;
				for (int j = 0; j < numobjectshit; j++)
				{
					if (hitobjects[j] == ents[i])
						balreadyhit = true;
				}
				if (balreadyhit)
					continue;

				// check so we are sufficiently close
				if (distance(ents[i]->pos, pos) > (phys_size * 2.0f))
					continue;

				// hit a player, give him damage and stuffs...
				game.create_sound(ents[i]->pos, SOUND_NINJA_HIT);
				// set his velocity to fast upward (for now)
				if(numobjectshit < 10)
					hitobjects[numobjectshit++] = ents[i];
					
				ents[i]->take_damage(vec2(0,10.0f), data->weapons.ninja.base->damage, player->client_id,WEAPON_NINJA);
			}
		}
		return 0;
	}

	return 0;
}


void CHARACTER::do_weaponswitch()
{
	if(reload_timer != 0) // make sure we have reloaded
		return;
		
	if(queued_weapon == -1) // check for a queued weapon
		return;

	if(weapons[WEAPON_NINJA].got) // if we have ninja, no weapon selection is possible
		return;

	// switch weapon
	set_weapon(queued_weapon);
}

void CHARACTER::handle_weaponswitch()
{
	int wanted_weapon = active_weapon;
	if(queued_weapon != -1)
		wanted_weapon = queued_weapon;
	
	// select weapon
	int next = count_input(latest_previnput.next_weapon, latest_input.next_weapon).presses;
	int prev = count_input(latest_previnput.prev_weapon, latest_input.prev_weapon).presses;

	if(next < 128) // make sure we only try sane stuff
	{
		while(next) // next weapon selection
		{
			wanted_weapon = (wanted_weapon+1)%NUM_WEAPONS;
			if(weapons[wanted_weapon].got)
				next--;
		}
	}

	if(prev < 128) // make sure we only try sane stuff
	{
		while(prev) // prev weapon selection
		{
			wanted_weapon = (wanted_weapon-1)<0?NUM_WEAPONS-1:wanted_weapon-1;
			if(weapons[wanted_weapon].got)
				prev--;
		}
	}

	// direct weapon selection
	if(latest_input.wanted_weapon)
		wanted_weapon = input.wanted_weapon-1;

	// check for insane values
	if(wanted_weapon >= 0 && wanted_weapon < NUM_WEAPONS && wanted_weapon != active_weapon && weapons[wanted_weapon].got)
		queued_weapon = wanted_weapon;
	
	do_weaponswitch();
}

void CHARACTER::fire_weapon()
{
	if(reload_timer != 0 || freeze_time > 0)
		return;
		
	do_weaponswitch();
	
	vec2 direction = normalize(vec2(latest_input.target_x, latest_input.target_y));
	
	bool fullauto = false;
	if(active_weapon == WEAPON_GRENADE || active_weapon == WEAPON_SHOTGUN || active_weapon == WEAPON_RIFLE)
		fullauto = true;


	// check if we gonna fire
	bool will_fire = false;
	if(count_input(latest_previnput.fire, latest_input.fire).presses) will_fire = true;
	if(fullauto && (latest_input.fire&1) && weapons[active_weapon].ammo) will_fire = true;
	if(!will_fire)
		return;
		
	// check for ammo
	if(!weapons[active_weapon].ammo)
	{
		// 125ms is a magical limit of how fast a human can click
		reload_timer = 125 * server_tickspeed() / 1000;;
		game.create_sound(pos, SOUND_WEAPON_NOAMMO);
		return;
	}
	
	vec2 projectile_startpos = pos+direction*phys_size*0.75f;
	
	switch(active_weapon)
	{
		case WEAPON_HAMMER:
		{
			// reset objects hit
			numobjectshit = 0;
			game.create_sound(pos, SOUND_HAMMER_FIRE);
			
			CHARACTER *ents[64];
			int hits = 0;
			int num = game.world.find_entities(pos+direction*phys_size*0.75f, phys_size*0.5f, (ENTITY**)ents, 64, NETOBJTYPE_CHARACTER);
 
			for (int i = 0; i < num; i++)
			{
				CHARACTER *target = ents[i];
				if (target == this)
					continue;
					
				// hit a player, give him damage and stuffs...
				vec2 fdir = normalize(ents[i]->pos - pos);

				// set his velocity to fast upward (for now)
				game.create_hammerhit(pos);
				
				ents[i]->take_damage(vec2(0,-1.0f), data->weapons.hammer.base->damage, player->client_id, active_weapon);
				
				vec2 dir;
				if (length(target->pos - pos) > 0.0f)
					dir = normalize(target->pos - pos);
				else
					dir = vec2(0,-1);
					
				target->core.vel += normalize(dir + vec2(0,-1.1f)) * 10.0f * (hammer_type+1);
				target->unfreeze();
				hits++;
			}
			
			// if we hit anything, we have to wait for the reload
			if(hits)
				reload_timer = server_tickspeed()/3;
		} break;

		case WEAPON_GUN:
		{
			PROJECTILE *proj = new PROJECTILE(WEAPON_GUN,
				player->client_id,
				projectile_startpos,
				direction,
				(int)(server_tickspeed()*tuning.gun_lifetime),
				0, 0, 0, -1, WEAPON_GUN);
				
			// pack the projectile and send it to the client directly
			NETOBJ_PROJECTILE p;
			proj->fill_info(&p);
			
			msg_pack_start(NETMSGTYPE_SV_EXTRAPROJECTILE, 0);
			msg_pack_int(1);
			for(unsigned i = 0; i < sizeof(NETOBJ_PROJECTILE)/sizeof(int); i++)
				msg_pack_int(((int *)&p)[i]);
			msg_pack_end();
			server_send_msg(player->client_id);
							
			game.create_sound(pos, SOUND_GUN_FIRE);
		} break;
		
		case WEAPON_SHOTGUN:
		{
			new LASER(pos, direction, tuning.laser_reach, player->client_id,1);
			game.create_sound(pos, SOUND_SHOTGUN_FIRE);
		} break;

		case WEAPON_GRENADE:
		{
			PROJECTILE *proj = new PROJECTILE(WEAPON_GRENADE,
				player->client_id,
				projectile_startpos,
				direction,
				(int)(server_tickspeed()*tuning.grenade_lifetime),
				false, PROJECTILE::PROJECTILE_FLAGS_EXPLODE, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);
			// pack the projectile and send it to the client directly
			NETOBJ_PROJECTILE p;
			proj->fill_info(&p);
			
			msg_pack_start(NETMSGTYPE_SV_EXTRAPROJECTILE, 0);
			msg_pack_int(1);
			for(unsigned i = 0; i < sizeof(NETOBJ_PROJECTILE)/sizeof(int); i++)
				msg_pack_int(((int *)&p)[i]);
			msg_pack_end();
			server_send_msg(player->client_id);

			game.create_sound(pos, SOUND_GRENADE_FIRE);
		} break;
		
		case WEAPON_RIFLE:
		{
			new LASER(pos, direction, tuning.laser_reach, player->client_id);
		} break;
		
		case WEAPON_NINJA:
		{
			attack_tick = server_tick();
			ninja.activationdir = direction;
			//ninja.currentmovetime = data->weapons.ninja.movetime * server_tickspeed() / 1000;
			ninja.currentmovetime = 10;
			//reload_timer = data->weapons.ninja.base->firedelay * server_tickspeed() / 1000 + server_tick();
			
			// reset hit objects
			numobjectshit = 0;

			//game.create_sound(pos, SOUND_NINJA_FIRE);
			
		} break;
		
	}

	attack_tick = server_tick();
	if(!reload_timer)
		reload_timer = data->weapons.id[active_weapon].firedelay * server_tickspeed() / 1000;
}

int CHARACTER::handle_weapons()
{
	vec2 direction = normalize(vec2(latest_input.target_x, latest_input.target_y));

	/*
	if(config.dbg_stress)
	{
		for(int i = 0; i < NUM_WEAPONS; i++)
		{
			weapons[i].got = true;
			weapons[i].ammo = 10;
		}

		if(reload_timer) // twice as fast reload
			reload_timer--;
	} */

	if(active_weapon == WEAPON_NINJA)
		handle_ninja();


	// check reload timer
	if(reload_timer)
	{
		reload_timer--;
		return 0;
	}
	
	/*
	if (active_weapon == WEAPON_NINJA)
	{
		// don't update other weapons while ninja is active
		return handle_ninja();
	}*/

	// fire weapon, if wanted
	fire_weapon();
	
	return 0;
}

void CHARACTER::on_predicted_input(NETOBJ_PLAYER_INPUT *new_input)
{
	// check for changes
	if(mem_comp(&input, new_input, sizeof(NETOBJ_PLAYER_INPUT)) != 0)
		last_action = server_tick();
		
	// copy new input
	mem_copy(&input, new_input, sizeof(input));
	num_inputs++;
	
	// or are not allowed to aim in the center
	if(input.target_x == 0 && input.target_y == 0)
		input.target_y = -1;	
}

void CHARACTER::on_direct_input(NETOBJ_PLAYER_INPUT *new_input)
{
	mem_copy(&latest_previnput, &latest_input, sizeof(latest_input));
	mem_copy(&latest_input, new_input, sizeof(latest_input));
	
	if(num_inputs > 2 && team != -1)
	{
		handle_weaponswitch();
		fire_weapon();
	}
	
	mem_copy(&latest_previnput, &latest_input, sizeof(latest_input));
}

void CHARACTER::tick()
{
	//endless drag
	if(config.sv_endless_hook)
		core.hook_tick = 0;

	if(player->force_balanced)
	{
		char buf[128];
		str_format(buf, sizeof(buf), "You were moved to %s due to team balancing", game.controller->get_team_name(team));
		game.send_broadcast(buf, player->client_id);
		
		player->force_balanced = false;
	}

	if(input.direction != 0 || input.jump != 0)
		lastmove = server_tick();
	
	if(freeze_time > 0)
	{
		if (freeze_time % (server_tickspeed())==0)
		{
			game.create_damageind(pos, 0, freeze_time/server_tickspeed());
		}
		freeze_time--;
		input.direction = 0;
		input.jump = 0;
		input.hook = 0;
		input.fire = 0;

		if (freeze_time-1==0)
		{
			unfreeze();
		}
	}
	
	core.input = input;
	core.tick(true);
	do_splash = false;

	if (super && core.jumped>1)
		core.jumped=1;
	


	//race

	char buftime[128];
	float time = (float)(server_tick()-starttime)/((float)server_tickspeed());
	

	if(server_tick()-refreshtime >= server_tickspeed())
	{
		if (race_state == RACE_STARTED)
		{
			int int_time = (int)time;
			if(config.sv_broadcast_time == 0 || int_time < config.sv_broadcast_time) {
				str_format(buftime, sizeof(buftime), "Current time: %d min %d sec\n%s", int_time/60,(int_time%60),(config.sv_broadcast[0]==0)?"":config.sv_broadcast);
			}
			else {
				str_format(buftime, sizeof(buftime), "Current time: %d min %d sec", int_time/60,(int_time%60));
			}
						
			game.send_broadcast(buftime, player->client_id);
		}
		else
		{
			if (config.sv_broadcast[0]!=0) {
				game.send_broadcast(config.sv_broadcast, player->client_id);
			}
		}
		refreshtime = server_tick();
	}

	if(col_is_begin(pos.x,pos.y) && race_state == RACE_NONE)
	{
		starttime = server_tick();
		refreshtime = server_tick();
		race_state = RACE_STARTED;
	}
	else if(col_is_end(pos.x, pos.y) && race_state == RACE_STARTED)
	{
		int new_rec = 0;
		char buf[128];
		if ((int)time/60 != 0)
			str_format(buf, sizeof(buf), "%s finished in: %d minute(s) %5.3f second(s)", (!config.sv_championship)?server_clientname(player->client_id):"You", (int)time/60, time-((int)time/60*60));
		else
			str_format(buf, sizeof(buf), "%s finished in: %5.3f second(s)", (!config.sv_championship)?server_clientname(player->client_id):"You", time-((int)time/60*60));
		if (!config.sv_championship)
			game.send_chat(-1,GAMECONTEXT::CHAT_ALL, buf);
		else
			game.send_chat_target(player->client_id, buf);

		PLAYER_SCORE *pscore = ((GAMECONTROLLER_RACE*)game.controller)->score.search_name(server_clientname(player->client_id));
		
		if(!pscore) new_rec = 1;
		
		if(pscore && time - pscore->score < 0)
		{
			new_rec = 1;
			str_format(buf, sizeof(buf), "New record: %5.3f second(s) better", time - pscore->score);
			if (!config.sv_championship)
				game.send_chat(-1,GAMECONTEXT::CHAT_ALL, buf);
			else
				game.send_chat_target(player->client_id, buf);
		}
		
		race_state = RACE_NONE;
		
		if(strncmp(server_clientname(player->client_id), "nameless tee", 12) != 0) {
			((GAMECONTROLLER_RACE*)game.controller)->score.parsePlayer(server_clientname(player->client_id), (float)time);
			
			if(!config.sv_championship) {
				PLAYER_SCORE *rank1_score = ((GAMECONTROLLER_RACE*)game.controller)->score.get_rank(1);
				if(rank1_score->score >= time) {
					// first finish or new top 1, reset all scores
					for(int i = 0; i < MAX_CLIENTS; i++) {
						if(game.players[i]) {
							PLAYER_SCORE *client_score = ((GAMECONTROLLER_RACE*)game.controller)->score.search_name(server_clientname(i));
							if(client_score == 0) {
								game.players[i]->score = -9999;
							} else {
								game.players[i]->score = (int)(time - client_score->score - 0.999);
							}
						}
					}
				} else if(new_rec) {
					player->score = (int)(rank1_score->score - time - 0.999);
				}
			}
		}
	}
	else if(col_is_kick(pos.x, pos.y) && !super)
	{
		if (player->authed>0)
			die(-1,WEAPON_WORLD);
		else
			server_kick(player->client_id, "You was kicked by kick zone");
	}
	else if(col_is_freeze(pos.x, pos.y) && !super)
	{
		freeze(server_tickspeed()*3);
	}
	else if(col_is_unfreeze(pos.x, pos.y) && !super)
	{
		unfreeze();
	}
	int booster=col_is_boost(pos.x, pos.y);
	core.vel+=boost_accel(booster);

	int z = col_is_teleport(pos.x, pos.y);
	if(z)
	{
		if (!core.super)
		{
			core.hooked_player = -1;
			core.hook_state = HOOK_RETRACTED;
			core.triggered_events |= COREEVENT_HOOK_RETRACT;
			core.hook_state = HOOK_RETRACTED;
			core.hook_pos = core.pos;
		}
		core.pos = teleport(z);
	}
	
	float phys_size = 28.0f;
	// handle death-tiles
	if((col_get((int)(pos.x+phys_size/2), (int)(pos.y-phys_size/2))&COLFLAG_DEATH ||
			col_get((int)(pos.x+phys_size/2), (int)(pos.y+phys_size/2))&COLFLAG_DEATH ||
			col_get((int)(pos.x-phys_size/2), (int)(pos.y-phys_size/2))&COLFLAG_DEATH ||
			col_get((int)(pos.x-phys_size/2), (int)(pos.y+phys_size/2))&COLFLAG_DEATH) && !super)
	{
		die(player->client_id, WEAPON_WORLD);
	}

	// handle weapons
	handle_weapons();

	player_state = input.player_state;

	// Previnput
	previnput = input;

	if (!doored)
	{
		olderpos = oldpos;
		oldpos = core.pos;
	}
	return;
}

float point_distance(vec2 point, vec2 line_start, vec2 line_end)
{
	float res = -1.0f;
	vec2 dir = normalize(line_end-line_start);
	for(int i = 0; i < length(line_end-line_start); i++)
	{
		vec2 step = dir;
		step.x *= i;
		step.y *= i;
		float dist = distance(step+line_start, point);
		if(res < 0 || dist < res)
			res = dist;
	}
	return res;
}

void CHARACTER::reset_pos()
{
	core.pos = olderpos;
	//core.pos-=core.vel;
	core.vel = vec2(0,0);
	
	if(core.jumped >= 2)
		core.jumped = 1;
}

void CHARACTER::tick_defered()
{

	// advance the dummy
	{
		WORLD_CORE tempworld;
		reckoningcore.world = &tempworld;
		reckoningcore.tick(false);
		reckoningcore.move();
		reckoningcore.quantize();
	}
	
	//lastsentcore;
	/*if(!dead)
	{*/

		vec2 start_pos = core.pos;
		vec2 start_vel = core.vel;
		bool stuck_before = test_box(core.pos, vec2(28.0f, 28.0f));
		

		core.move();

		if (doored)
		{
			reset_pos();
			doored=false;
		}

		bool stuck_after_move = test_box(core.pos, vec2(28.0f, 28.0f));
		core.quantize();
		bool stuck_after_quant = test_box(core.pos, vec2(28.0f, 28.0f));
		pos = core.pos;
		
		if(!stuck_before && (stuck_after_move || stuck_after_quant))
		{
			/*dbg_msg("player", "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x", 
				stuck_before,
				stuck_after_move,
				stuck_after_quant,
				start_pos.x, start_pos.y,
				start_vel.x, start_vel.y,
				*((unsigned *)&start_pos.x), *((unsigned *)&start_pos.y),
				*((unsigned *)&start_vel.x), *((unsigned *)&start_vel.y));*/
		}

		int events = core.triggered_events;
		int mask = cmask_all_except_one(player->client_id);
		
		if(events&COREEVENT_GROUND_JUMP) game.create_sound(pos, SOUND_PLAYER_JUMP, mask);
		
		//if(events&COREEVENT_HOOK_LAUNCH) snd_play_random(CHN_WORLD, SOUND_HOOK_LOOP, 1.0f, pos);
		if(events&COREEVENT_HOOK_ATTACH_PLAYER) game.create_sound(pos, SOUND_HOOK_ATTACH_PLAYER, cmask_all());
		if(events&COREEVENT_HOOK_ATTACH_GROUND) game.create_sound(pos, SOUND_HOOK_ATTACH_GROUND, mask);
		if(events&COREEVENT_HOOK_HIT_NOHOOK) game.create_sound(pos, SOUND_HOOK_NOATTACH, mask);
		//if(events&COREEVENT_HOOK_RETRACT) snd_play_random(CHN_WORLD, SOUND_PLAYER_JUMP, 1.0f, pos);
	//}
	
	if(team == -1)
	{
		pos.x = input.target_x;
		pos.y = input.target_y;
	}
	
	// update the sendcore if needed
	{
		NETOBJ_CHARACTER predicted;
		NETOBJ_CHARACTER current;
		mem_zero(&predicted, sizeof(predicted));
		mem_zero(&current, sizeof(current));
		reckoningcore.write(&predicted);
		core.write(&current);

		// only allow dead reackoning for a top of 3 seconds
		if(core.p_reset || reckoning_tick+server_tickspeed()*3 < server_tick() || mem_comp(&predicted, &current, sizeof(NETOBJ_CHARACTER)) != 0)
		{
			reckoning_tick = server_tick();
			sendcore = core;
			reckoningcore = core;
			core.p_reset = false;
		}
	}
}

bool CHARACTER::increase_health(int amount)
{
	if(health >= 10)
		return false;
	health = clamp(health+amount, 0, 10);
	return true;
}

bool CHARACTER::increase_armor(int amount)
{
	if(armor >= 10)
		return false;
	armor = clamp(armor+amount, 0, 10);
	return true;
}

bool CHARACTER::freeze(int time)
{
	if (time<=1 || super)
		return false;
	if (freeze_tick<server_tick()-server_tickspeed())
	{
		if(active_weapon != WEAPON_NINJA) {
			unfreezed_weapon = active_weapon;
		}

		ninja.activationtick = server_tick();
		weapons[WEAPON_NINJA].got = true;
		weapons[WEAPON_NINJA].ammo = -1;
		last_weapon = active_weapon;
		active_weapon = WEAPON_NINJA;
		freeze_tick=server_tick();
		freeze_time=time;
		return true;
	}
	return false;
}

bool CHARACTER::unfreeze()
{
	if (freeze_time>0)
	{
		freeze_tick=0;
		freeze_time=0;
		weapons[WEAPON_NINJA].got = false;
		active_weapon = last_weapon;
		if(active_weapon == WEAPON_NINJA) {
			active_weapon = unfreezed_weapon;
		}

		set_weapon(active_weapon);
		return true;
	}
	return false;
}

void CHARACTER::give_all_weapons()
{
		for(int i=1;i<NUM_WEAPONS-1;i++)
		{
			weapons[i].got=true;
			weapons[i].ammo=-1;
		}
		return;
}


void CHARACTER::die(int killer, int weapon)
{
	/*if (dead || team == -1)
		return;*/
	int mode_special = game.controller->on_character_death(this, game.players[killer], weapon);

	dbg_msg("game", "kill killer='%d:%s' victim='%d:%s' weapon=%d special=%d",
		killer, server_clientname(killer),
		player->client_id, server_clientname(player->client_id), weapon, mode_special);

	// send the kill message
	NETMSG_SV_KILLMSG msg;
	msg.killer = killer;
	msg.victim = player->client_id;
	msg.weapon = weapon;
	msg.mode_special = mode_special;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(-1);

	// a nice sound
	game.create_sound(pos, SOUND_PLAYER_DIE);

	// set dead state
	// TODO: do stuff here
	/*
	die_pos = pos;
	dead = true;
	*/
	
	// this is for auto respawn after 3 secs
	player->die_tick = server_tick();
	
	alive = false;
	game.world.remove_entity(this);
	game.world.core.characters[player->client_id] = 0;
	game.create_death(pos, player->client_id);
	
	// we got to wait 0.5 secs before respawning
	player->respawn_tick = server_tick()+server_tickspeed()/2;
}

bool CHARACTER::take_damage(vec2 force, int dmg, int from, int weapon)
{
	core.vel += force;
	return true;

	//game.create_damageind(pos, 0, dmg);

	//damage_taken_tick = server_tick();

	//game.create_sound(game.players[from]->view_pos, SOUND_HIT, cmask_one(from));


	//game.create_sound(pos, SOUND_PLAYER_PAIN_LONG);
	//game.create_sound(pos, SOUND_PLAYER_PAIN_SHORT);

	//emote_type = EMOTE_PAIN;
	//emote_stop = server_tick() + 500 * server_tickspeed() / 1000;


}

void CHARACTER::snap(int snapping_client)
{
	if(networkclipped(snapping_client))
		return;
	
	NETOBJ_CHARACTER *character = (NETOBJ_CHARACTER *)snap_new_item(NETOBJTYPE_CHARACTER, player->client_id, sizeof(NETOBJ_CHARACTER));
	
	// write down the core
	if(game.world.paused)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		character->tick = 0;
		core.write(character);
	}
	else
	{
		character->tick = reckoning_tick;
		sendcore.write(character);
	}

	if(do_splash)
		character->jumped = 3;

	// set emote
	if (emote_stop < server_tick())
	{
		emote_type = EMOTE_NORMAL;
		emote_stop = -1;
	}

	character->emote = emote_type;

	character->ammocount = 0;
	character->health = 0;
	character->armor = 0;
	
	character->weapon = active_weapon;
	character->attacktick = attack_tick;

	character->direction = input.direction;

	if(player->client_id == snapping_client)
	{
		character->health = health;
		character->armor = armor;
		if(weapons[active_weapon].ammo > 0)
			character->ammocount = weapons[active_weapon].ammo;
	}

	if (character->emote == EMOTE_NORMAL)
	{
		if(250 - ((server_tick() - last_action)%(250)) < 5)
			character->emote = EMOTE_BLINK;
	}

	character->player_state = player_state;
}
