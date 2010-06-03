/* copyright (c) 2007 rajh, race mod stuff */
#include <engine/e_server_interface.h>
#include <game/mapitems.hpp>
#include <game/server/entities/character.hpp>
#include <game/server/player.hpp>
#include <game/server/gamecontext.hpp>
#include "race.hpp"

GAMECONTROLLER_RACE::GAMECONTROLLER_RACE()
{
	gametype = "DDRace";
}

void GAMECONTROLLER_RACE::tick()
{
	GAMECONTROLLER::tick();
}
