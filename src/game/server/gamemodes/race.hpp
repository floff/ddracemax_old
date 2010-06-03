/* copyright (c) 2007 rajh and gregwar. Score stuff */

#ifndef RACE_H
#define RACE_H
#include <game/server/gamecontroller.hpp>
#include <game/server/score.hpp>

class GAMECONTROLLER_RACE : public GAMECONTROLLER
{
public:
	GAMECONTROLLER_RACE();

	SCORE score;

	virtual void tick();
};

#endif
