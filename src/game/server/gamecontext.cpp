#include <string.h>
#include <new>
#include <engine/e_config.h>
#include <engine/e_server_interface.h>
#include "gamecontext.hpp"

GAMECONTEXT game;

GAMECONTEXT::GAMECONTEXT()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		players[i] = 0;
	
	controller = 0;
	vote_closetime = 0;
	tunes = true;
}

GAMECONTEXT::~GAMECONTEXT()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		delete players[i];
}

void GAMECONTEXT::clear()
{
	bool cheats = this->cheats;
	bool tunes = this->tunes;
	this->~GAMECONTEXT();
	mem_zero(this, sizeof(*this));
	new (this) GAMECONTEXT();
	this->cheats = cheats;
	this->tunes = tunes;
}


class CHARACTER *GAMECONTEXT::get_player_char(int client_id)
{
	if(client_id < 0 || client_id >= MAX_CLIENTS || !players[client_id])
		return 0;
	return players[client_id]->get_character();
}

void GAMECONTEXT::create_damageind(vec2 p, float angle, int amount)
{
	float a = 3 * 3.14159f / 2 + angle;
	//float a = get_angle(dir);
	float s = a-pi/3;
	float e = a+pi/3;
	for(int i = 0; i < amount; i++)
	{
		float f = mix(s, e, float(i+1)/float(amount+2));
		NETEVENT_DAMAGEIND *ev = (NETEVENT_DAMAGEIND *)events.create(NETEVENTTYPE_DAMAGEIND, sizeof(NETEVENT_DAMAGEIND));
		if(ev)
		{
			ev->x = (int)p.x;
			ev->y = (int)p.y;
			ev->angle = (int)(f*256.0f);
		}
	}
}

void GAMECONTEXT::create_hammerhit(vec2 p)
{
	// create the event
	NETEVENT_HAMMERHIT *ev = (NETEVENT_HAMMERHIT *)events.create(NETEVENTTYPE_HAMMERHIT, sizeof(NETEVENT_HAMMERHIT));
	if(ev)
	{
		ev->x = (int)p.x;
		ev->y = (int)p.y;
	}
}


void GAMECONTEXT::create_explosion(vec2 p, int owner, int weapon, bool bnodamage)
{
	// create the event
	NETEVENT_EXPLOSION *ev = (NETEVENT_EXPLOSION *)events.create(NETEVENTTYPE_EXPLOSION, sizeof(NETEVENT_EXPLOSION));
	if(ev)
	{
		ev->x = (int)p.x;
		ev->y = (int)p.y;
	}

	if (!bnodamage)
	{
		// deal damage
		CHARACTER *ents[64];
		float radius = 135.0f;
		float innerradius = 48.0f;
		int num = game.world.find_entities(p, radius, (ENTITY**)ents, 64, NETOBJTYPE_CHARACTER);
		for(int i = 0; i < num; i++)
		{
			vec2 diff = ents[i]->pos - p;
			vec2 forcedir(0,1);
			float l = length(diff);
			if(l)
				forcedir = normalize(diff);
			l = 1-clamp((l-innerradius)/(radius-innerradius), 0.0f, 1.0f);
			float dmg = 6 * l;
			if((int)dmg)
				ents[i]->take_damage(forcedir*dmg*2, (int)dmg, owner, weapon);
		}
	}
}

/*
void create_smoke(vec2 p)
{
	// create the event
	EV_EXPLOSION *ev = (EV_EXPLOSION *)events.create(EVENT_SMOKE, sizeof(EV_EXPLOSION));
	if(ev)
	{
		ev->x = (int)p.x;
		ev->y = (int)p.y;
	}
}*/

void GAMECONTEXT::create_playerspawn(vec2 p)
{
	// create the event
	NETEVENT_SPAWN *ev = (NETEVENT_SPAWN *)events.create(NETEVENTTYPE_SPAWN, sizeof(NETEVENT_SPAWN));
	if(ev)
	{
		ev->x = (int)p.x;
		ev->y = (int)p.y;
	}
}

void GAMECONTEXT::create_death(vec2 p, int cid)
{
	// create the event
	NETEVENT_DEATH *ev = (NETEVENT_DEATH *)events.create(NETEVENTTYPE_DEATH, sizeof(NETEVENT_DEATH));
	if(ev)
	{
		ev->x = (int)p.x;
		ev->y = (int)p.y;
		ev->cid = cid;
	}
}

void GAMECONTEXT::create_sound(vec2 pos, int sound, int mask)
{
	if (sound < 0)
		return;

	// create a sound
	NETEVENT_SOUNDWORLD *ev = (NETEVENT_SOUNDWORLD *)events.create(NETEVENTTYPE_SOUNDWORLD, sizeof(NETEVENT_SOUNDWORLD), mask);
	if(ev)
	{
		ev->x = (int)pos.x;
		ev->y = (int)pos.y;
		ev->soundid = sound;
	}
}

void GAMECONTEXT::create_sound_global(int sound, int target)
{
	if (sound < 0)
		return;

	NETMSG_SV_SOUNDGLOBAL msg;
	msg.soundid = sound;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(target);
}


void GAMECONTEXT::send_chat_target(int to, const char *text)
{
	NETMSG_SV_CHAT msg;
	msg.team = 0;
	msg.cid = -1;
	msg.message = text;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(to);
}


void GAMECONTEXT::send_chat(int chatter_cid, int team, const char *text)
{
	/*
	if(chatter_cid >= 0 && chatter_cid < MAX_CLIENTS)
		dbg_msg("chat", "%d:%d:%s: %s", chatter_cid, team, server_clientname(chatter_cid), text);
	else
		dbg_msg("chat", "*** %s", text);
	*/

	if(team == CHAT_ALL)
	{
		NETMSG_SV_CHAT msg;
		msg.team = 0;
		msg.cid = chatter_cid;
		msg.message = text;
		msg.pack(MSGFLAG_VITAL);
		server_send_msg(-1);
	}
	else
	{
		NETMSG_SV_CHAT msg;
		msg.team = 1;
		msg.cid = chatter_cid;
		msg.message = text;
		msg.pack(MSGFLAG_VITAL);

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(game.players[i] && game.players[i]->team == team)
				server_send_msg(i);
		}
	}
}

void GAMECONTEXT::send_emoticon(int cid, int emoticon)
{
	NETMSG_SV_EMOTICON msg;
	msg.cid = cid;
	msg.emoticon = emoticon;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(-1);
}

void GAMECONTEXT::send_weapon_pickup(int cid, int weapon)
{
	NETMSG_SV_WEAPONPICKUP msg;
	msg.weapon = weapon;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(cid);
}


void GAMECONTEXT::send_broadcast(const char *text, int cid)
{
	NETMSG_SV_BROADCAST msg;
	msg.message = text;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(cid);
}

// 
void GAMECONTEXT::start_vote(const char *desc, const char *command)
{
	// check if a vote is already running
	if(vote_closetime)
		return;

	// reset votes
	vote_enforce = VOTE_ENFORCE_UNKNOWN;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(players[i])
			players[i]->vote = 0;
	}
	
	// start vote
	vote_closetime = time_get() + time_freq()*25;
	str_copy(vote_description, desc, sizeof(vote_description));
	str_copy(vote_command, command, sizeof(vote_description));
	send_vote_set(-1);
	send_vote_status(-1);
}


void GAMECONTEXT::end_vote()
{
	vote_closetime = 0;
	send_vote_set(-1);
}

void GAMECONTEXT::send_vote_set(int cid)
{
	NETMSG_SV_VOTE_SET msg;
	if(vote_closetime)
	{
		msg.timeout = (vote_closetime-time_get())/time_freq();
		msg.description = vote_description;
		msg.command = vote_command;
	}
	else
	{
		msg.timeout = 0;
		msg.description = "";
		msg.command = "";
	}
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(cid);
}

void GAMECONTEXT::send_vote_status(int cid)
{
	NETMSG_SV_VOTE_STATUS msg = {0};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(players[i])
		{
			msg.total++;
			if(players[i]->vote > 0)
				msg.yes++;
			else if(players[i]->vote < 0)
				msg.no++;
			else
				msg.pass++;
		}
	}	

	msg.pack(MSGFLAG_VITAL);
	server_send_msg(cid);
	
}

void GAMECONTEXT::abort_vote_kick_on_disconnect(int client_id)
{
	if(vote_closetime && !strncmp(vote_command, "kick ", 5) && atoi(&vote_command[5]) == client_id)
		vote_closetime = -1;
}

void GAMECONTEXT::tick()
{
	world.core.tuning = tuning;
	world.tick();

	//if(world.paused) // make sure that the game object always updates
	controller->tick();
		
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(players[i])
			players[i]->tick();
	}
	
	// update voting
	if(vote_closetime)
	{
		// abort the kick-vote on player-leave
		if(vote_closetime == -1)
		{
			send_chat(-1, GAMECONTEXT::CHAT_ALL, "Vote aborted");
			end_vote();
		}
		else
		{
			// count votes
			int total = 0, yes = 0, no = 0;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(players[i])
				{
					total++;
					if(players[i]->vote > 0)
						yes++;
					else if(players[i]->vote < 0)
						no++;
				}
			}
		
			if(vote_enforce == VOTE_ENFORCE_YES || yes >= total/2+1 || (time_get() > vote_closetime && yes>no))
			{
				bool kicked=false;
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					char buf[11];
					str_format(buf, sizeof(buf), "ban %d %d", i,config.sv_vote_kick_bantime);
					if (vote_command == buf)
						if (players[i]->authed==0)
							{
								console_execute_line(vote_command, 4,-1);
								end_vote();
								send_chat(-1, GAMECONTEXT::CHAT_ALL, "Vote passed");
								kicked=true;
							}
				}
				if (!kicked)
				{
					end_vote();
					send_chat(-1, GAMECONTEXT::CHAT_ALL, "Vote failed can't kick authenticated users.");
				}
				
			
				if(players[vote_creator])
					players[vote_creator]->last_votecall = 0;
			}
			else if(vote_enforce == VOTE_ENFORCE_NO || time_get() > vote_closetime || no >= total/2+1 || yes+no == total)
			{
				end_vote();
				send_chat(-1, GAMECONTEXT::CHAT_ALL, "Vote failed");
			}
		}
	}
}

void GAMECONTEXT::snap(int client_id)
{
	world.snap(client_id);
	controller->snap(client_id);
	events.snap(client_id);
	
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(players[i])
			players[i]->snap(client_id);
	}
}
