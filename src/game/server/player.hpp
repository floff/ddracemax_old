#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

// this include should perhaps be removed
#include "entities/character.hpp"

// player object
class PLAYER
{
	MACRO_ALLOC_POOL_ID()
private:
	CHARACTER *character;
public:
	PLAYER(int client_id);
	~PLAYER();

	//DDRace
	int muted;
	//int hammer_ type;

	// TODO: clean this up
	int authed;
	int resistent;
	char skin_name[64];
	int use_custom_color;
	int color_body;
	int color_feet;
	bool color_set; // Set if player changed color at least once
	
	int respawn_tick;
	int die_tick;
	//
	bool spawning;
	int client_id;
	int team;
	int score;
	bool force_balanced;
	
	//
	int vote;
	int64 last_votecall;

	int64 last_kickvote;

	//
	int64 last_chat;
	int64 last_setteam;
	int64 last_changeinfo;
	int64 last_emote;
	int64 last_kill;

	// network latency calculations	
	struct
	{
		int accum;
		int accum_min;
		int accum_max;
		int avg;
		int min;
		int max;	
	} latency;
	
	// this is used for snapping so we know how we can clip the view for the player
	vec2 view_pos;

	void init(int client_id);
	
	CHARACTER *get_character();
	
	void kill_character(int weapon);

	void try_respawn();
	void respawn();
	void set_team(int team);
	
	void tick();
	void snap(int snapping_client);

	void on_direct_input(NETOBJ_PLAYER_INPUT *new_input);
	void on_predicted_input(NETOBJ_PLAYER_INPUT *new_input);
	void on_disconnect();

	//race var
	int starttime;
	int refreshtime;
	int race_state;
	int besttick;
	int lasttick;
	float bestLap;
	
	// afk timer
	void afk_timer(int new_target_x, int new_target_y);
	int64 last_playtime;
	int last_target_x;
	int last_target_y;
	int sent_afk_warning; // afk timer's 1st warning after 50% of sv_max_afk_time
	int sent_afk_warning2; // afk timer's 2nd warning after 90% of sv_max_afk_time
	char afk_msg[160];


};

#endif
