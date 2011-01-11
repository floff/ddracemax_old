/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <base/system.h>
#include <base/math.hpp>
#include <base/vmath.hpp>

#include <math.h>
#include <engine/e_common_interface.h>
#include <game/mapitems.hpp>
#include <game/layers.hpp>
#include <game/collision.hpp>

static TILE *tiles;
static int width = 0;
static int height = 0;
//race
static int *dest[95];
static int len[95];
static int tele[95];

static int *teleporter;

int col_width() { return width; }
int col_height() { return height; }

int col_init()
{
	width = layers_game_layer()->width;
	height = layers_game_layer()->height;
	tiles = (TILE *)map_get_data(layers_game_layer()->data);

	//race
	mem_zero(&len, sizeof(len));
	mem_zero(&tele, sizeof(tele));
	teleporter = new int[width*height];

	for(int i = width*height-1; i >= 0; i--)
	{
		if(tiles[i].index > 34 && tiles[i].index < 190)
		{
			if(tiles[i].index&1)
				len[tiles[i].index >> 1]++;
			else if(!(tiles[i].index&1))
				tele[(tiles[i].index-1) >> 1]++;
		}
	}
	for(int i = 0; i < 95; i++)
	{
		dest[i] = new int[len[i]];
		len[i] = 0;
	}
	for(int i = width*height-1; i >= 0; i--)
	{
		if(tiles[i].index&1 && tiles[i].index > 34 && tiles[i].index < 190)
			dest[tiles[i].index>>1][len[tiles[i].index>>1]++] = i;
	}

	for(int i = 0; i < width*height; i++)
	{
		int index = tiles[i].index;
		teleporter[i] = 0;
		
		if(index > 190)
			continue;
		
		if(index == TILE_DEATH)
			tiles[i].index = COLFLAG_DEATH;
		else if(index == TILE_SOLID)
			tiles[i].index = COLFLAG_SOLID;
		else if(index == TILE_NOHOOK)
			tiles[i].index = COLFLAG_SOLID|COLFLAG_NOHOOK;
		else if(index == TILE_NOLASER)
			tiles[i].index = COLFLAG_NOLASER;
		else
		{
			tiles[i].index = 0;
			teleporter[i] = index;
		}
	}
				
	return 1;
}


int col_get(int x, int y)
{
	int nx = clamp(x/32, 0, width-1);
	int ny = clamp(y/32, 0, height-1);
	
	if(tiles[ny*width+nx].index > 128)
		return 0;
	return tiles[ny*width+nx].index;
}

int col_is_solid(int x, int y)
{
	return (col_get(x,y)&COLFLAG_SOLID);
}

int col_is_through(int x, int y)
{
	/*
	 * TODO
	 * Make one function to get tiles like that
	 * It hurts to look at it
	 */

	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;

	int tile = teleporter[ny*width+nx];
	
	return tile == TILE_THROUGH || tile == TILE_THROUGHF;
}

int col_is_nolaser(int x, int y)
{
	return (col_get(x,y)&COLFLAG_NOLASER);
}

//race
int col_is_teleport(int x, int y)
{
	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;

	int z = teleporter[ny*width+nx]-1;
	if(z > 34 && z < 190 && z&1)
		return z>>1;
	return 0;
}

int col_is_checkpoint(int x, int y)
{
	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;

	int z = teleporter[ny*width+nx];
	if(z > 34 && z < 190 && z&1 && tele[z>>1] <= 0)
		return z>>1;
	return 0;
}

int col_is_begin(int x, int y)
{
	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;
	
	return teleporter[ny*width+nx] == TILE_BEGIN;
}

int col_is_end(int x, int y)
{
	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;
	
	return teleporter[ny*width+nx] == TILE_END;
}

int col_is_freeze(int x, int y)
{
	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;

	int tile = teleporter[ny*width+nx];
	
	return tile == TILE_FREEZE ||
		(tile >= TILE_BOOST_FL && tile <= TILE_THROUGHF) ||
		(tile >= TILE_BOOST_FL2 && tile <= TILE_BOOST_FU2);
}

int col_is_unfreeze(int x, int y)
{
	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;
	
	return teleporter[ny*width+nx] == TILE_UNFREEZE;
}

int col_is_kick(int x, int y)
{
	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;
	
	return teleporter[ny*width+nx] == TILE_KICK;
}

int col_is_ehook_start(int x, int y)
{
	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;

	int tile = teleporter[ny*width+nx];
	
	return tile == TILE_EHOOK_START;
}

int col_is_ehook_end(int x, int y)
{
	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;
	
	return teleporter[ny*width+nx] == TILE_EHOOK_END;
}

int col_is_boost(int x, int y)
{
	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;

	int tile = teleporter[ny*width+nx];

	if	(
			(tile >= TILE_BOOST_L && tile <= TILE_BOOST_U) || 
			(tile >= TILE_BOOST_L2 && tile <= TILE_BOOST_U2) ||
			(tile >= TILE_BOOST_FL && tile <= TILE_BOOST_FU) || 
			(tile >= TILE_BOOST_FL2 && tile <= TILE_BOOST_FU2)
		)
		return tile;
	return 0;
}



vec2 boost_accel(int index)
{
	if (index==TILE_BOOST_L || index==TILE_BOOST_FL)
		return vec2(-3,0);
	else if (index==TILE_BOOST_R || index==TILE_BOOST_FR)
		return vec2(3,0);
	else if (index==TILE_BOOST_D || index==TILE_BOOST_FD)
		return vec2(0,2);
	else if (index==TILE_BOOST_U || index==TILE_BOOST_FU)
		return vec2(0,-2);
	else if (index==TILE_BOOST_L2 || index==TILE_BOOST_FL2)
		return vec2(-15,0);
	else if (index==TILE_BOOST_R2 || index==TILE_BOOST_FR2)
		return vec2(15,0);
	else if (index==TILE_BOOST_D2 || index==TILE_BOOST_FD2)
		return vec2(0,15);
	else if (index==TILE_BOOST_U2 || index==TILE_BOOST_FU2)
		return vec2(0,-15);

	return vec2(0,0);
}


vec2 teleport(int a)
{
	if(len[a] > 0)
	{
		int r = rand()%len[a];
		int x = (dest[a][r]%width)<<5;
		int y = (dest[a][r]/width)<<5;
		return vec2((float)x+16.0, (float)y+16.0);
	}
	else
		return vec2(0, 0);
}

void col_set(int x, int y, int flag)
{
	int nx = clamp(x/32, 0, width-1);
	int ny = clamp(y/32, 0, height-1);
	
	tiles[ny*width+nx].index = flag;
}

void col_through_offset(vec2 pos0, vec2 pos1, int *ox, int *oy)
{
	float x = pos0.x - pos1.x;
	float y = pos0.y - pos1.y;
	if (fabs(x) > fabs(y))
	{
		if (x < 0)
		{
			*ox = -32;
			*oy = 0;
		}
		else
		{
			*ox = 32;
			*oy = 0;
		}
	}
	else
	{
		if (y < 0)
		{
			*ox = 0;
			*oy = -32;
		}
		else
		{
			*ox = 0;
			*oy = 32;
		}
	}
}

// TODO: rewrite this smarter!
int col_intersect_line(vec2 pos0, vec2 pos1, vec2 *out_collision, vec2 *out_before_collision, bool allow_through)
{
	float d = distance(pos0, pos1);
	vec2 last = pos0;
	int ix, iy; // Temporary position for checking collision
	int dx, dy; // Offset for checking the "through" tile

	if (allow_through)
	{
		col_through_offset(pos0, pos1, &dx, &dy);
	}
	
	for(float f = 0; f < d; f++)
	{
		float a = f/d;
		vec2 pos = mix(pos0, pos1, a);
		ix = round(pos.x);
		iy = round(pos.y);
		if(col_is_solid(ix, iy) && !(allow_through && col_is_through(ix + dx, iy + dy)))
		{
			if(out_collision)
				*out_collision = pos;
			if(out_before_collision)
				*out_before_collision = last;
			return col_get(ix, iy);
		}
		last = pos;
	}
	if(out_collision)
		*out_collision = pos1;
	if(out_before_collision)
		*out_before_collision = pos1;
	return 0;
}

int col_intersect_nolaser(vec2 pos0, vec2 pos1, vec2 *out_collision, vec2 *out_before_collision)
{
	float d = distance(pos0, pos1);
	vec2 last = pos0;
	
	for(float f = 0; f < d; f++)
	{
		float a = f/d;
		vec2 pos = mix(pos0, pos1, a);
		if(col_is_solid(round(pos.x), round(pos.y)) || col_is_nolaser(round(pos.x), round(pos.y)))
		{
			if(out_collision)
				*out_collision = pos;
			if(out_before_collision)
				*out_before_collision = last;
			return col_get(round(pos.x), round(pos.y));
		}
		last = pos;
	}
	if(out_collision)
		*out_collision = pos1;
	if(out_before_collision)
		*out_before_collision = pos1;
	return 0;
}

int col_intersect_air(vec2 pos0, vec2 pos1, vec2 *out_collision, vec2 *out_before_collision)
{
	float d = distance(pos0, pos1);
	vec2 last = pos0;
	
	for(float f = 0; f < d; f++)
	{
		float a = f/d;
		vec2 pos = mix(pos0, pos1, a);
		if(col_is_solid(round(pos.x), round(pos.y)) || !col_get(round(pos.x), round(pos.y)))
		{
			if(out_collision)
				*out_collision = pos;
			if(out_before_collision)
				*out_before_collision = last;
			if(!col_get(round(pos.x), round(pos.y)))
				return -1;
			else
				return col_get(round(pos.x), round(pos.y));
		}
		last = pos;
	}
	if(out_collision)
		*out_collision = pos1;
	if(out_before_collision)
		*out_before_collision = pos1;
	return 0;
}

