/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#ifndef GAME_MAPRES_COL_H
#define GAME_MAPRES_COL_H

#include <base/vmath.hpp>

enum
{
	COLFLAG_SOLID=1,
	COLFLAG_DEATH=2,
	COLFLAG_NOHOOK=4,
	COLFLAG_NOLASER=8,

};

int col_init();
int col_is_solid(int x, int y);
void col_set(int x, int y, int flag);
int col_get(int x, int y);
int col_width();
int col_height();
int col_intersect_line(vec2 pos0, vec2 pos1, vec2 *out_collision, vec2 *out_before_collision);
int col_intersect_nolaser(vec2 pos0, vec2 pos1, vec2 *out_collision, vec2 *out_before_collision);
int col_intersect_air(vec2 pos0, vec2 pos1, vec2 *out_collision, vec2 *out_before_collision);

//race
int col_is_teleport(int x, int y);
int col_is_checkpoint(int x, int y);
int col_is_begin(int x, int y);
int col_is_nolaser(int x, int y);
int col_is_end(int x, int y);
int col_is_boost(int x, int y);
int col_is_freeze(int x, int y);
int col_is_unfreeze(int x, int y);
int col_is_kick(int x, int y);
vec2 boost_accel(int index);
vec2 teleport(int z);
#endif
