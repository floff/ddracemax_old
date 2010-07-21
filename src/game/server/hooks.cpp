/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cstring> // strncmp

#include <base/math.hpp>

#include <engine/e_config.h>
#include <engine/e_server_interface.h>
extern "C"
{
	#include <engine/e_memheap.h>
}
#include <game/version.hpp>
#include <game/collision.hpp>
#include <game/layers.hpp>

#include <game/gamecore.hpp>

#include "gamecontext.hpp"
#include "gamemodes/race.hpp"
#include <engine/e_protocol.h>

static void server_send_rcon_line(int cid, const char *line)
{
	msg_pack_start_system(NETMSG_RCON_LINE, MSGFLAG_VITAL);
	msg_pack_string(line, 512);
	msg_pack_end();
	server_send_msg(cid);
}

TUNING_PARAMS tuning;

static void check_pure_tuning()
{
	// might not be created yet during start up
	if(!game.controller)
		return;
	
	if(	strcmp(game.controller->gametype, "DM")==0 ||
		strcmp(game.controller->gametype, "TDM")==0 ||
		strcmp(game.controller->gametype, "CTF")==0)
	{
		TUNING_PARAMS p;
		if(memcmp(&p, &tuning, sizeof(TUNING_PARAMS)) != 0)
		{
			dbg_msg("server", "resetting tuning due to pure server");
			tuning = p;
		}
	}	
}

struct VOTEOPTION
{
	VOTEOPTION *next;
	VOTEOPTION *prev;
	char command[1];
};

static HEAP *voteoption_heap = 0;
static VOTEOPTION *voteoption_first = 0;
static VOTEOPTION *voteoption_last = 0;

void send_tuning_params(int cid)
{
	check_pure_tuning();
	
	msg_pack_start(NETMSGTYPE_SV_TUNEPARAMS, MSGFLAG_VITAL);
	int *params = (int *)&tuning;
	for(unsigned i = 0; i < sizeof(tuning)/sizeof(int); i++)
		msg_pack_int(params[i]);
	msg_pack_end();
	server_send_msg(cid);
}

bool compare_players(PLAYER *pl1, PLAYER *pl2)
{
	if (pl1->authed>pl2->authed)
		return true;
	else
		return false;
}


// Server hooks
void mods_client_direct_input(int client_id, void *input)
{
	if(!game.world.paused)
		game.players[client_id]->on_direct_input((NETOBJ_PLAYER_INPUT *)input);
}

void mods_set_authed(int client_id, int status)
{
	if(game.players[client_id])
		game.players[client_id]->authed = status;
}

void mods_set_resistent(int client_id, int status)
{
	if(game.players[client_id])
		game.players[client_id]->resistent = status;
}

void mods_client_predicted_input(int client_id, void *input)
{
	if(!game.world.paused)
		game.players[client_id]->on_predicted_input((NETOBJ_PLAYER_INPUT *)input);
}

// Server hooks
void mods_tick()
{
	check_pure_tuning();
	
	game.tick();
}

void mods_snap(int client_id)
{
	game.snap(client_id);
}

void mods_client_enter(int client_id)
{
	//game.world.insert_entity(&game.players[client_id]);
	game.players[client_id]->respawn();
	dbg_msg("game", "join player='%d:%s'", client_id, server_clientname(client_id));

	game.players[client_id]->score = 0;
	
	//if(game.controller->is_race())
	//	((GAMECONTROLLER_RACE*)game.controller)->score.initPlayer(client_id);

	char buf[512];
	str_format(buf, sizeof(buf), "%s entered and joined the %s", server_clientname(client_id), game.controller->get_team_name(game.players[client_id]->team));
	game.send_chat(-1, GAMECONTEXT::CHAT_ALL, buf); 
	game.send_chat_target(client_id, config.sv_welcome);

	dbg_msg("game", "team_join player='%d:%s' team=%d", client_id, server_clientname(client_id), game.players[client_id]->team);
}

void mods_connected(int client_id)
{
	game.players[client_id] = new(client_id) PLAYER(client_id);

	//game.players[client_id].init(client_id);
	//game.players[client_id].client_id = client_id;
	
	// Check which team the player should be on
	if(config.sv_tournament_mode)
		game.players[client_id]->team = -1;
	else
		game.players[client_id]->team = game.controller->get_auto_team(client_id);

	// send motd
	NETMSG_SV_MOTD msg;
	msg.message = config.sv_motd;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(client_id);
}

void mods_client_drop(int client_id)
{
	game.abort_vote_kick_on_disconnect(client_id);
	game.players[client_id]->on_disconnect();
	delete game.players[client_id];
	game.players[client_id] = 0;
}

/*static bool is_separator(char c) { return c == ';' || c == ' ' || c == ',' || c == '\t'; }

static const char *liststr_find(const char *str, const char *needle)
{
	int needle_len = strlen(needle);
	while(*str)
	{
		int wordlen = 0;
		while(str[wordlen] && !is_separator(str[wordlen]))
			wordlen++;
		
		if(wordlen == needle_len && strncmp(str, needle, needle_len) == 0)
			return str;
		
		str += wordlen+1;
	}
	
	return 0;
}*/

void mods_message(int msgtype, int client_id)
{
	void *rawmsg = netmsg_secure_unpack(msgtype);
	PLAYER *p = game.players[client_id];
	
	if(!rawmsg)
	{
		dbg_msg("server", "dropped weird message '%s' (%d), failed on '%s'", netmsg_get_name(msgtype), msgtype, netmsg_failed_on());
		return;
	}
	
	if(msgtype == NETMSGTYPE_CL_SAY)
	{
		NETMSG_CL_SAY *msg = (NETMSG_CL_SAY *)rawmsg;
		int team = msg->team;
		if(team)
			team = p->team;
		else
			team = GAMECONTEXT::CHAT_ALL;
		
		if((config.sv_spamprotection && p->last_chat+time_freq() < time_get()) || !config.sv_spamprotection)
		{
			if(str_length(msg->message)>370)
			game.send_chat_target(client_id,"Your Message is too long");
			else if(msg->message[0] == '/')
			{	
				if(!str_comp_nocase(msg->message, "/info"))
				{
					game.send_chat_target(client_id, "DDRace mod by 3DA");
					game.send_chat_target(client_id,"Version: " RACE_VERSION);
					game.send_chat_target(client_id, "Visit: http://fluxid.pl/teeworlds/ddrace2/");
					game.send_chat_target(client_id, "or Visit: http://github.com/fluxid/ddrace");
				}
				else if(!strncmp(msg->message, "/top5", 5) && !config.sv_championship) 
				{
					const char *pt = msg->message;
					int number = 0;
					pt += 6;
					while(*pt && *pt >= '0' && *pt <= '9')
					{
						number = number*10+(*pt-'0');
						pt++;
					}
					if(number)
						((GAMECONTROLLER_RACE*)game.controller)->score.top5_draw(client_id, number);
					else
						((GAMECONTROLLER_RACE*)game.controller)->score.top5_draw(client_id, 0);
				}
				else if(!str_comp_nocase(msg->message, "/rank"))
				{
					char buf[512];
					const char *name = msg->message;
					name += 6;
					int pos=0;
					PLAYER_SCORE *pscore;
					
					pscore = ((GAMECONTROLLER_RACE*)game.controller)->score.search_name(server_clientname(client_id), pos);

					if(pscore && pos > -1 && pscore->score != -1)
					{
						float time = pscore->score;

						str_format(buf, sizeof(buf), "%d. %s",!config.sv_championship?pos:0, pscore->name);
						if (!config.sv_championship)
							game.send_chat(-1, GAMECONTEXT::CHAT_ALL, buf);
						else
							game.send_chat_target(client_id, buf);

						if ((int)time/60 >= 1)
							str_format(buf, sizeof(buf), "Time: %d minute%s %f seconds", (int)time/60, (int)time/60 != 1 ? "s" : "" , time-((int)time/60)*60);
						else
							str_format(buf, sizeof(buf), "Time: %f seconds", time-((int)time/60)*60);
					}
					else
						str_format(buf, sizeof(buf), "%s is not ranked", strcmp(msg->message, "/rank")?name:server_clientname(client_id));

					game.send_chat_target(client_id, buf);
				}
				else
					game.send_chat_target(client_id, "No such command!");
				
			}
			else
				if (game.players[client_id]->muted==0)
					game.send_chat(client_id, team, msg->message);
				else
					game.send_chat_target(client_id, "You are muted yet.");
			game.players[client_id]->last_chat = time_get();
		}
	}
	else if(msgtype == NETMSGTYPE_CL_CALLVOTE)
		{
		int64 now = time_get();
		if(game.vote_closetime)
		{
			game.send_chat_target(client_id, "Wait for current vote to end before calling a new one.");
			return;
		}
		
		int64 timeleft = p->last_votecall + time_freq()*60 - now;
		if(timeleft > 0)
		{
			char chatmsg[512] = {0};
			str_format(chatmsg, sizeof(chatmsg), "You must wait %d seconds before making another vote", (timeleft/time_freq())+1);
			game.send_chat_target(client_id, chatmsg);
			return;
		}
		
		char chatmsg[512] = {0};
		char desc[512] = {0};
		char cmd[512] = {0};
		NETMSG_CL_CALLVOTE *msg = (NETMSG_CL_CALLVOTE *)rawmsg;
		if(str_comp_nocase(msg->type, "option") == 0)
		{
			if (config.sv_votes==1)
			{
				VOTEOPTION *option = voteoption_first;
				static int64 last_mapvote = 0;
				while(option)
				{
					if(str_comp_nocase(msg->value, option->command) == 0)
					{
						if(game.players[client_id]->authed == 0 && strncmp(option->command, "sv_map ", 7) == 0 && time_get() < last_mapvote + (time_freq() * config.sv_vote_map_delay)) {
							char chatmsg[512] = {0};
							str_format(chatmsg, sizeof(chatmsg), "There's a %d second delay between map-votes", config.sv_vote_map_delay);
							game.send_chat_target(client_id, chatmsg);
							return;
						}

						str_format(chatmsg, sizeof(chatmsg), "%s called vote to change server option '%s'", server_clientname(client_id), option->command);
						str_format(desc, sizeof(desc), "%s", option->command);
						str_format(cmd, sizeof(cmd), "%s", option->command);
						break;
					}

					option = option->next;
				}
				
				if(!option)
				{
					str_format(chatmsg, sizeof(chatmsg), "'%s' isn't an option on this server", msg->value);
					game.send_chat_target(client_id, chatmsg);
					return;
				}

				last_mapvote = time_get();
			}
		}
		else if(str_comp_nocase(msg->type, "kick") == 0)
		{
			if(game.players[client_id]->authed == 0 && time_get() < game.players[client_id]->last_kickvote + (time_freq() * config.sv_vote_kick_delay)) {
				char chatmsg[512] = {0};
				str_format(chatmsg, sizeof(chatmsg), "There's a %d second delay between kick-votes", config.sv_vote_kick_delay);
				game.send_chat_target(client_id, chatmsg);
				return;
			}

			if(!config.sv_vote_kick)
			{
				game.send_chat_target(client_id, "Server does not allow voting to kick players");
				return;
			}
			int kick_id = atoi(msg->value);
			if(kick_id < 0 || kick_id >= MAX_CLIENTS || !game.players[kick_id])
			{
				game.send_chat_target(client_id, "Invalid client id to kick");
				return;
			}
			if(game.players[kick_id]->authed>0)
			{
				game.send_chat_target(client_id, "Server does not allow voting to kick admins");
				return;
			}
			
			str_format(chatmsg, sizeof(chatmsg), "%s called for vote to kick '%s'", server_clientname(client_id), server_clientname(kick_id));
			str_format(desc, sizeof(desc), "Kick '%s'", server_clientname(kick_id));
			str_format(cmd, sizeof(cmd), "kick %d", kick_id);
			if (!config.sv_vote_kick_bantime)
				str_format(cmd, sizeof(cmd), "kick %d", kick_id);
			else
				str_format(cmd, sizeof(cmd), "ban %d %d", kick_id, config.sv_vote_kick_bantime);

			game.players[client_id]->last_kickvote = time_get();
		}
		
		if(cmd[0])
		{
			game.send_chat(-1, GAMECONTEXT::CHAT_ALL, chatmsg);
			game.start_vote(desc, cmd);
			p->vote = 1;
			game.vote_creator = client_id;
			p->last_votecall = now;
			game.send_vote_status(-1);
		}
	}
	else if(msgtype == NETMSGTYPE_CL_VOTE)
	{
		if(!game.vote_closetime)
			return;

		if(p->vote == 0)
		{
			NETMSG_CL_VOTE *msg = (NETMSG_CL_VOTE *)rawmsg;
			p->vote = msg->vote;
			game.send_vote_status(-1);
			char chatmsg[128];
			char eventname[32];
			if (p->vote ==-1)
				str_format(eventname,sizeof(eventname),"no");
			else if (p->vote ==1)
				str_format(eventname,sizeof(eventname),"yes");
			str_format(chatmsg, sizeof(chatmsg), "%s votes %s", server_clientname(client_id),eventname);
			game.send_chat(-1, GAMECONTEXT::CHAT_ALL, chatmsg);
		}
	}
	else if (msgtype == NETMSGTYPE_CL_SETTEAM && !game.world.paused)
	{
		NETMSG_CL_SETTEAM *msg = (NETMSG_CL_SETTEAM *)rawmsg;
		
		if(config.sv_spamprotection && p->last_setteam+time_freq()*3 > time_get())
			return;

		// Switch team on given client and kill/respawn him
		if(game.controller->can_join_team(msg->team, client_id))
		{
			p->last_setteam = time_get();
			p->set_team(msg->team);
		}
		else
		{
			char buf[128];
			str_format(buf, sizeof(buf), "Only %d active players are allowed", config.sv_max_clients-config.sv_spectator_slots);
			game.send_broadcast(buf, client_id);
		}
	}
	else if (msgtype == NETMSGTYPE_CL_CHANGEINFO || msgtype == NETMSGTYPE_CL_STARTINFO)
	{
		NETMSG_CL_CHANGEINFO *msg = (NETMSG_CL_CHANGEINFO *)rawmsg;
		
		if(config.sv_spamprotection && p->last_changeinfo+time_freq()*5 > time_get())
			return;
			
		p->last_changeinfo = time_get();
		
		if (!p->color_set || config.sv_allow_color_change)
		{
			p->use_custom_color = msg->use_custom_color;
			p->color_body = msg->color_body;
			p->color_feet = msg->color_feet;
			p->color_set = true;
		}

		// check for invalid chars
		unsigned char *name = (unsigned char *)msg->name;
		while (*name)
		{
			if(*name < 32)
				*name = ' ';
			name++;
		}

		// copy old name
		char oldname[MAX_NAME_LENGTH];
		str_copy(oldname, server_clientname(client_id), MAX_NAME_LENGTH);
		server_setclientname(client_id, msg->name);
		if(msgtype == NETMSGTYPE_CL_CHANGEINFO && strcmp(oldname, server_clientname(client_id)) != 0)
		{
			char chattext[256];
			str_format(chattext, sizeof(chattext), "%s changed name to %s", oldname, server_clientname(client_id));
			game.send_chat(-1, GAMECONTEXT::CHAT_ALL, chattext);
		}
		
		// set skin
		str_copy(p->skin_name, msg->skin, sizeof(p->skin_name));
		
		if(msgtype == NETMSGTYPE_CL_STARTINFO)
		{
			// send vote options
			NETMSG_SV_VOTE_CLEAROPTIONS clearmsg;
			clearmsg.pack(MSGFLAG_VITAL);
			server_send_msg(client_id);
			VOTEOPTION *current = voteoption_first;
			while(current)
			{
				NETMSG_SV_VOTE_OPTION optionmsg;
				optionmsg.command = current->command;
				optionmsg.pack(MSGFLAG_VITAL);
				server_send_msg(client_id);
				current = current->next;
			}
			
			// send tuning parameters to client
			send_tuning_params(client_id);

			//
			NETMSG_SV_READYTOENTER m;
			m.pack(MSGFLAG_VITAL|MSGFLAG_FLUSH);
			server_send_msg(client_id);
		}
	}
	else if (msgtype == NETMSGTYPE_CL_EMOTICON && !game.world.paused)
	{
		NETMSG_CL_EMOTICON *msg = (NETMSG_CL_EMOTICON *)rawmsg;
		
		if(config.sv_spamprotection && p->last_emote+time_freq()*3 > time_get())
			return;
			
		p->last_emote = time_get();
		
		game.send_emoticon(client_id, msg->emoticon);
	}
	else if (msgtype == NETMSGTYPE_CL_KILL && !game.world.paused)
	{
		if(p->last_kill+time_freq()*3 > time_get())
			return;
		
		p->last_kill = time_get();
		p->kill_character(WEAPON_SELF);
		p->respawn_tick = server_tick()+server_tickspeed()*3;
		if(game.controller->is_race())
			p->respawn_tick = server_tick();
	}
}

static void con_tune_param(void *result, void *user_data, int cid)
{
	if (game.tunes)
	{
		const char *param_name = console_arg_string(result, 0);
		float new_value = console_arg_float(result, 1);

		if(tuning.set(param_name, new_value))
		{
			dbg_msg("tuning", "%s changed to %.2f", param_name, new_value);
			send_tuning_params(-1);
		}
		else
			server_send_rcon_line(cid, "No such tuning parameter");
	}		
}

static void con_tune_reset(void *result, void *user_data, int cid)
{
	if (game.tunes)
	{
		TUNING_PARAMS p;
		tuning = p;
		send_tuning_params(-1);
		server_send_rcon_line(cid, "tuning reset");
	}
}

static void con_tune_dump(void *result, void *user_data, int cid)
{
	for(int i = 0; i < tuning.num(); i++)
	{
		float v;
		tuning.get(i, &v);
		char buf[512];
		str_format(buf, sizeof(buf), "%s %.2f", tuning.names[i], v);
			server_send_rcon_line(cid, buf);
	}
}

static void con_ninja_me(void *result, void *user_data, int cid)
{
	if (!game.cheats)
		return;
	CHARACTER* chr = game.players[cid]->get_character();
	if(chr)
	{
		chr->ninja.activationtick = server_tick();
		chr->weapons[WEAPON_NINJA].got = true;
		chr->weapons[WEAPON_NINJA].ammo = -1;
		chr->last_weapon = chr->active_weapon;
		chr->active_weapon = WEAPON_NINJA;
		game.create_sound(chr->pos, SOUND_PICKUP_NINJA);
		chr->race_state = RACE_CHEAT;
	}
}

static void con_ninja(void *result, void *user_data, int cid)
{
	if (!game.cheats)
		return;
	int cid1 = clamp(console_arg_int(result, 0), 0, (int)MAX_CLIENTS-1);
	if (game.players[cid1] && compare_players(game.players[cid],game.players[cid1]))
	{
		CHARACTER* chr = game.players[cid1]->get_character();
		if(chr)
		{
			chr->ninja.activationtick = server_tick();
			chr->weapons[WEAPON_NINJA].got = true;
			chr->weapons[WEAPON_NINJA].ammo = -1;
			chr->last_weapon = chr->active_weapon;
			chr->active_weapon = WEAPON_NINJA;
			game.create_sound(chr->pos, SOUND_PICKUP_NINJA);
			chr->race_state = RACE_CHEAT;
		}
	}
}

static void con_go_left(void *result, void *user_data, int cid)
{
	if (!game.cheats)
		return;
	CHARACTER* chr = game.players[cid]->get_character();
	if(chr)
	{
		chr->core.pos.x -= 32;
		chr->race_state = RACE_CHEAT;
	}
}

static void con_go_right(void *result, void *user_data, int cid)
{
	if (!game.cheats)
		return;
	CHARACTER* chr = game.players[cid]->get_character();
	if(chr)
	{
		chr->core.pos.x += 32;
		chr->race_state = RACE_CHEAT;
	}
}

static void con_go_up(void *result, void *user_data, int cid)
{
	if (!game.cheats)
		return;
	CHARACTER* chr = game.players[cid]->get_character();
	if(chr)
	{
		chr->core.pos.y -= 32;
		chr->race_state = RACE_CHEAT;
	}
}

static void con_go_down(void *result, void *user_data, int cid)
{
	if (!game.cheats)
		return;
	CHARACTER* chr = game.players[cid]->get_character();
	if(chr)
	{
		chr->core.pos.y += 32;
		chr->race_state = RACE_CHEAT;
	}
}

static void con_hammer(void *result, void *user_data, int cid)
{
	if (!game.cheats)
		return;
	char buf[128];
	CHARACTER* chr = game.players[cid]->get_character();
	if (!chr)
		return;
	int type = console_arg_int(result, 0);
	if (type>3 || type<0)
	{
		server_send_rcon_line(cid, "Select hammer between 0 and 3");
	}
	else if (!chr->super)
		server_send_rcon_line(cid, "Use 'super' command to allow this command");
	else
	{
		chr->hammer_type = type;
		str_format(buf, sizeof(buf), "Hammer seted to %d",type);
		server_send_rcon_line(cid, buf);
	}
}

static void con_weapons(void *result, void *user_data, int cid)
{
	if (!game.cheats)
		return;
	int cid1 = clamp(console_arg_int(result, 0), 0, (int)MAX_CLIENTS-1);
	if (game.players[cid1] && compare_players(game.players[cid],game.players[cid1]))
	{
		CHARACTER* chr = game.players[cid1]->get_character();
		if(chr)
		{
			chr->give_all_weapons();
			chr->race_state=RACE_CHEAT;
		}
	}
}

static void con_weapons_me(void *result, void *user_data, int cid)
{
	if (!game.cheats)
		return;

	CHARACTER* chr = game.players[cid]->get_character();
	if(chr)
	{
		chr->give_all_weapons();
		chr->race_state=RACE_CHEAT;
	}
}

static void con_restart(void *result, void *user_data, int cid)
{
	if(console_arg_num(result))
		game.controller->do_warmup(console_arg_int(result, 0));
	else
		game.controller->startround();
}

static void con_say(void *result, void *user_data, int cid)
{
	game.send_chat(-1, GAMECONTEXT::CHAT_ALL, console_arg_string(result, 0));
}

static void con_set_team(void *result, void *user_data, int cid)
{
	int client_id = clamp(console_arg_int(result, 0), 0, (int)MAX_CLIENTS-1);
	int team = clamp(console_arg_int(result, 1), -1, 1);
		
	if(game.players[client_id] && compare_players(game.players[cid],game.players[client_id]))
	{
		game.players[client_id]->set_team(team);
	}
}

static void con_kill_pl(void *result, void *user_data, int cid)
{
	int cid1 = clamp(console_arg_int(result, 0), 0, (int)MAX_CLIENTS-1);
	if(!game.players[cid1])
		return;
		
	if (game.players[cid1] && compare_players(game.players[cid],game.players[cid1]))
	{
		game.players[cid1]->kill_character(WEAPON_GAME);
		char buf[512];
		str_format(buf, sizeof(buf), "%s killed by %s", server_clientname(cid1), server_clientname(cid));
		game.send_chat(-1, GAMECONTEXT::CHAT_ALL, buf);
	}
}



static void con_teleport(void *result, void *user_data, int cid)
{	
	if (!game.cheats)
		return;
 	int cid1 = clamp(console_arg_int(result, 0), 0, (int)MAX_CLIENTS-1);
 	int cid2 = clamp(console_arg_int(result, 1), 0, (int)MAX_CLIENTS-1);
	if(game.players[cid1] && game.players[cid2])
	{
		if (cid==cid1
			|| (compare_players(game.players[cid],game.players[cid1]) &&  compare_players(game.players[cid],game.players[cid2]))
			|| (compare_players(game.players[cid],game.players[cid1]) && cid2==cid))
		{
			CHARACTER* chr = game.players[cid1]->get_character();
			if(chr)
			{
				chr->core.pos = game.players[cid2]->view_pos;
				chr->race_state=RACE_CHEAT;
			}
		}
	}
}

static void con_super(void *result, void *user_data, int cid)
{	
	if (!game.cheats)
		return;
	int cid1 = clamp(console_arg_int(result, 0), 0, (int)MAX_CLIENTS-1);
	if (game.players[cid1] && compare_players(game.players[cid], game.players[cid1]))
	{
		CHARACTER* chr = game.players[cid1]->get_character();
		if(chr)
		{
			chr->super=true;
			chr->core.super=true;
			chr->race_state=RACE_CHEAT;
		}
	}
}

static void con_super_me(void *result, void *user_data, int cid)
{	
	if (!game.cheats)
		return;
	CHARACTER* chr = game.players[cid]->get_character();
	if(chr)
	{
		chr->super=true;
		chr->core.super=true;
		chr->race_state=RACE_CHEAT;
	}
}

static void con_setlvl(void *result, void *user_data, int cid)
{	
	int cid1 = clamp(console_arg_int(result, 0), 0, (int)MAX_CLIENTS-1);
	int level = clamp(console_arg_int(result, 1), 0, 3);
	if (game.players[cid1] && (game.players[cid]->authed>level) && (compare_players(game.players[cid],game.players[cid1])))
	{
		game.players[cid1]->authed=level;
	}
}

static void con_mute(void *result, void *user_data, int cid)
{	
	int cid1 = clamp(console_arg_int(result, 0), 0, (int)MAX_CLIENTS-1);
	int seconds = console_arg_int(result, 1);
	char buf[512];
	if (seconds<10)
		seconds=10;
	if (game.players[cid1] && (compare_players(game.players[cid],game.players[cid1])))
	{
		if (game.players[cid1]->muted<seconds*server_tickspeed())
			game.players[cid1]->muted=seconds*server_tickspeed();
		else
			seconds=game.players[cid1]->muted/server_tickspeed();
		str_format(buf, sizeof(buf), "%s muted by %s for %d seconds", server_clientname(cid1), server_clientname(cid), seconds);
		game.send_chat(-1, GAMECONTEXT::CHAT_ALL, buf);

	}
}

static void con_addvote(void *result, void *user_data, int cid)
{
	int len = strlen(console_arg_string(result, 0));
	
	if(!voteoption_heap)
		voteoption_heap = memheap_create();
	
	VOTEOPTION *option = (VOTEOPTION *)memheap_allocate(voteoption_heap, sizeof(VOTEOPTION) + len);
	option->next = 0;
	option->prev = voteoption_last;
	if(option->prev)
		option->prev->next = option;
	voteoption_last = option;
	if(!voteoption_first)
		voteoption_first = option;
	
	mem_copy(option->command, console_arg_string(result, 0), len+1);
	dbg_msg("server", "added option '%s'", option->command);
}

static void con_vote(void *result, void *user_data, int cid)
{
	if(str_comp_nocase(console_arg_string(result, 0), "yes") == 0)
		game.vote_enforce = GAMECONTEXT::VOTE_ENFORCE_YES;
	else if(str_comp_nocase(console_arg_string(result, 0), "no") == 0)
		game.vote_enforce = GAMECONTEXT::VOTE_ENFORCE_NO;
	dbg_msg("server", "forcing vote %s", console_arg_string(result, 0));
}

static void con_cheats_on(void *result, void *user_data, int cid)
{
	game.cheats=true;
}

static void con_tuning_off(void *result, void *user_data, int cid)
{
	game.tunes=false;
}
static void con_rcon_tries_bantime(void *result, void *user_data, int cid)
{
		int val = console_arg_int(result, 0);
		config.sv_rcon_tries_bantime=val;
		
}

static void con_rcon_tries(void *result, void *user_data, int cid)
{
		int val = console_arg_int(result, 0);
		config.sv_rcon_tries=val;
		
}

void mods_console_init()
{
	MACRO_REGISTER_COMMAND("sv_rcon_tries", "i", CFGFLAG_SERVER, con_rcon_tries , 0, "How Many Password Tries Before ban",3);
	MACRO_REGISTER_COMMAND("sv_rcon_tries_bantime", "i", CFGFLAG_SERVER, con_rcon_tries_bantime , 0, "How Much time will the brute rcon password attacker will be banned",3);
	MACRO_REGISTER_COMMAND("tune", "si", CFGFLAG_SERVER, con_tune_param, 0, "",3);
	MACRO_REGISTER_COMMAND("tune_reset", "", CFGFLAG_SERVER, con_tune_reset, 0, "",3);
	MACRO_REGISTER_COMMAND("tune_dump", "", CFGFLAG_SERVER, con_tune_dump, 0, "",3);

	MACRO_REGISTER_COMMAND("restart", "?i", CFGFLAG_SERVER, con_restart, 0, "",3);
	MACRO_REGISTER_COMMAND("say", "r", CFGFLAG_SERVER, con_say, 0, "",3);
	MACRO_REGISTER_COMMAND("set_team", "ii", CFGFLAG_SERVER, con_set_team, 0, "",2);

	MACRO_REGISTER_COMMAND("addvote", "r", CFGFLAG_SERVER, con_addvote, 0, "",4);
	MACRO_REGISTER_COMMAND("vote", "r", CFGFLAG_SERVER, con_vote, 0, "",3);
	
	MACRO_REGISTER_COMMAND("tele", "ii", CFGFLAG_SERVER, con_teleport, 0, "",2);
	MACRO_REGISTER_COMMAND("teleport", "ii", CFGFLAG_SERVER, con_teleport, 0, "",2);

	MACRO_REGISTER_COMMAND("left", "", CFGFLAG_SERVER, con_go_left, 0, "",1);
	MACRO_REGISTER_COMMAND("right", "", CFGFLAG_SERVER, con_go_right, 0, "",1);
	MACRO_REGISTER_COMMAND("up", "", CFGFLAG_SERVER, con_go_up, 0, "",1);
	MACRO_REGISTER_COMMAND("down", "", CFGFLAG_SERVER, con_go_down, 0, "",1);
	MACRO_REGISTER_COMMAND("weapons", "i", CFGFLAG_SERVER, con_weapons, 0, "",2);
	MACRO_REGISTER_COMMAND("weapons_me", "", CFGFLAG_SERVER, con_weapons_me, 0, "",1);
	MACRO_REGISTER_COMMAND("super", "i", CFGFLAG_SERVER, con_super, 0, "",2);
	MACRO_REGISTER_COMMAND("super_me", "", CFGFLAG_SERVER, con_super_me, 0, "",1);
	MACRO_REGISTER_COMMAND("hammer", "i", CFGFLAG_SERVER, con_hammer, 0, "",1);
	MACRO_REGISTER_COMMAND("ninja", "i", CFGFLAG_SERVER, con_ninja, 0, "",2);
	MACRO_REGISTER_COMMAND("ninja_me", "", CFGFLAG_SERVER, con_ninja_me, 0, "",1);

	MACRO_REGISTER_COMMAND("kill_pl", "i", CFGFLAG_SERVER, con_kill_pl, 0, "",2);

	MACRO_REGISTER_COMMAND("auth", "ii", CFGFLAG_SERVER, con_setlvl, 0, "",3);
	MACRO_REGISTER_COMMAND("mute", "ii", CFGFLAG_SERVER, con_mute, 0, "",2);
	MACRO_REGISTER_COMMAND("cheats_on", "", CFGFLAG_SERVER, con_cheats_on, 0, "",4);
	MACRO_REGISTER_COMMAND("tuning_off", "", CFGFLAG_SERVER, con_tuning_off, 0, "",4);
}

void mods_init()
{
	//if(!data) /* only load once */
		//data = load_data_from_memory(internal_data);
		
	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		snap_set_staticsize(i, netobj_get_size(i));

	layers_init();
	col_init();

	// reset everything here
	//world = new GAMEWORLD;
	//players = new PLAYER[MAX_CLIENTS];

	char buf[512];

	str_format(buf, sizeof(buf), "data/maps/%s.cfg", config.sv_map);
	console_execute_file(buf);

	str_format(buf, sizeof(buf), "data/maps/%s.map.cfg", config.sv_map);
	console_execute_file(buf);

	// select gametype
	game.controller = new GAMECONTROLLER_RACE;

	// setup core world
	//for(int i = 0; i < MAX_CLIENTS; i++)
	//	game.players[i].core.world = &game.world.core;

	// create all entities from the game layer
	MAPITEM_LAYER_TILEMAP *tmap = layers_game_layer();
	TILE *tiles = (TILE *)map_get_data(tmap->data);
	
	for(int y = 0; y < tmap->height; y++)
	{
		for(int x = 0; x < tmap->width; x++)
		{
			int index = tiles[y*tmap->width+x].index;
			
			if(index >= ENTITY_OFFSET)
			{
				game.controller->on_entity(index-ENTITY_OFFSET, x ,y);
				//game.controller->on_entity(game.controller->get_index(x,y), pos, x ,y);
			}
		}
	}
	//
	//game.world.insert_entity(game.controller);
}

void mods_shutdown()
{
	delete game.controller;
	game.controller = 0;
	game.clear();
}

void mods_presnap() {}
void mods_postsnap()
{
	game.events.clear();
}

extern "C" const char *mods_net_version() { return GAME_NETVERSION; }
extern "C" const char *mods_version() { return GAME_VERSION; }
