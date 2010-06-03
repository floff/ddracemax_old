/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */

#ifndef GUN_TYPE
#define GUN_TYPE

#include <game/server/entity.hpp>
#include <game/gamecore.hpp>

class CHARACTER;

class GUN : public ENTITY
{
	int eval_tick;

	void fire();
	
public:
	GUN(vec2 pos);

	virtual void reset();
	virtual void tick();
	virtual void snap(int snapping_client);
};


#endif