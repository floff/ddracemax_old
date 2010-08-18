/* copyright (c) 2008 rajh and gregwar. Score stuff */

#ifndef SCORE_H_RACE
#define SCORE_H_RACE
#include <engine/e_server_interface.h>

class PLAYER_SCORE
{
public:
	char name[MAX_NAME_LENGTH];
	float score;

	PLAYER_SCORE(const char *name, float score);

	bool operator==(const PLAYER_SCORE& other) { return (this->score == other.score); }
	bool operator<(const PLAYER_SCORE& other) { return (this->score < other.score); }
};

class SCORE
{
public:
	SCORE();
	
	void save();
	void load();
	PLAYER_SCORE *search_name(const char *name, int &pos);
	PLAYER_SCORE *search_name(const char *name);
	PLAYER_SCORE *get_rank(int rank);
	void parsePlayer(const char *name, float score);
	void top5_draw(int id, int debut);
};

#endif
