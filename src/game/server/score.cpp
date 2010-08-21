/* copyright (c) 2008 rajh and gregwar. Score stuff */

#include "score.hpp"
#include "gamecontext.hpp"
#include <string.h>
#include <sstream>
#include <fstream>
#include <list>
#include <engine/e_config.h>
#include <engine/e_server_interface.h>

PLAYER_SCORE::PLAYER_SCORE(const char *name, float score)
{
	str_copy(this->name, name, sizeof(this->name));
	this->score = score;
}

std::list<PLAYER_SCORE> top;

SCORE::SCORE()
{
	load();
}

std::string save_file()
{
	std::ostringstream oss;
	oss << config.sv_map << "_record.dtb";
	return oss.str();
}

void SCORE::save()
{
	std::fstream f;
	f.open(save_file().c_str(), std::ios::out);
	if(!f.fail()) {
		for(std::list<PLAYER_SCORE>::iterator i=top.begin(); i!=top.end(); i++)
		{
			f << i->name << std::endl << i->score << std::endl;
		}
	}
	f.close();
}

void SCORE::load()
{
	std::fstream f;
	f.open(save_file().c_str(), std::ios::in);
	top.clear();
	while (!f.eof() && !f.fail())
	{
		std::string tmpname, tmpscore;
		std::getline(f, tmpname);
		if(!f.eof() && tmpname != "")
		{
			std::getline(f, tmpscore);
			top.push_back(*new PLAYER_SCORE(tmpname.c_str(), atof(tmpscore.c_str())));
		}
	}
	f.close();
}

PLAYER_SCORE *SCORE::search_name(const char *name, int &pos)
{
	pos=0;
	for (std::list<PLAYER_SCORE>::iterator i = top.begin(); i!=top.end(); i++)
	{
		pos++;
		if (!strcmp(i->name, name))
		{
			return & (*i);
		}
	}
	pos=-1;
	return 0;
}

PLAYER_SCORE *SCORE::search_name(const char *name)
{
	for (std::list<PLAYER_SCORE>::iterator i = top.begin(); i!=top.end(); i++)
	{
		if (!strcmp(i->name, name))
		{
			return & (*i);
		}
	}
	return 0;
}

PLAYER_SCORE *SCORE::get_rank(int rank) {
	rank--;
	int c=0;
	
	for (std::list<PLAYER_SCORE>::iterator i = top.begin(); i!=top.end(); i++,c++)
	{
		if(c == rank) {
			return & (*i);
		}
	}
	
	return 0;
}


void SCORE::parsePlayer(const char *name, float score)
{
	load();
	PLAYER_SCORE *player=search_name(name);
	if (player)
	{
		if (player->score>score)
		{
			player->score=score;
			top.sort();
			save();
		}
	}
	else
	{
		top.push_back(*new PLAYER_SCORE(name, score));
		top.sort();
		save();
	}
}

void SCORE::top5_draw(int id, int debut)
{
	int pos = 1;
	//char buf[512];
	game.send_chat_target(id, "----------- Top 5 -----------");
	for (std::list<PLAYER_SCORE>::iterator i = top.begin(); i != top.end() && pos <= 5+debut; i++)
	{
		if(i->score < 0)
			continue;

		if(pos >= debut)
		{
			std::ostringstream oss;
			oss << pos << ". " << i->name << "   Time: ";

			if ((int)(i->score)/60 != 0)
				oss << (int)(i->score)/60 << " minute(s) ";
			if (i->score-((int)i->score/60)*60 != 0)
				oss << i->score-((int)i->score/60)*60 <<" second(s)";

			game.send_chat_target(id, oss.str().c_str());
		}
		pos++;
	}
	game.send_chat_target(id, "-----------------------------");
}
