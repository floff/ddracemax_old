/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */

#ifndef GAME_SERVER_ENTITY_PROJECTILE_H
#define GAME_SERVER_ENTITY_PROJECTILE_H

class PROJECTILE : public ENTITY
{
public:
	enum
	{
		PROJECTILE_FLAGS_EXPLODE = 1 << 0,
	};
	
	vec2 direction;
	int lifespan;
	int owner;
	int type;
	int flags;
	int sound_impact;
	int weapon;
	float force;
	int start_tick;
	int bouncing;
	bool freeze;
	bool collised;
	
	PROJECTILE(int type, int owner, vec2 pos, vec2 vel, int span,
		bool freeze, int flags, float force, int sound_impact, int weapon);

	vec2 get_pos(float time);
	void fill_info(NETOBJ_PROJECTILE *proj);

	virtual void reset();
	virtual void tick();
	virtual void snap(int snapping_client);
};

#endif
