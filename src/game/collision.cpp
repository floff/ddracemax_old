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
	
	return teleporter[ny*width+nx] == TILE_FREEZE;
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

int col_is_boost(int x, int y)
{
	int nx = x/32;
	int ny = y/32;
	if(y < 0 || nx < 0 || nx >= width || ny >= height)
		return 0;
	if ((teleporter[ny*width+nx] >= TILE_BOOST_L && teleporter[ny*width+nx] <= TILE_BOOST_U) || (teleporter[ny*width+nx] >= TILE_BOOST_L2 && teleporter[ny*width+nx] <= TILE_BOOST_U2))
		return teleporter[ny*width+nx];
	return 0;
}



vec2 boost_accel(int index)
{
	if (index==TILE_BOOST_L)
		return vec2(-3,0);
	else if(index==TILE_BOOST_R)
		return vec2(3,0);
	else if(index==TILE_BOOST_D)
		return vec2(0,2);
	else if(index==TILE_BOOST_U)
		return vec2(0,-2);
	else if(index==TILE_BOOST_L2)
		return vec2(-15,0);
	else if(index==TILE_BOOST_R2)
		return vec2(15,0);
	else if(index==TILE_BOOST_D2)
		return vec2(0,15);
	else if(index==TILE_BOOST_U2)
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

// TODO: rewrite this smarter!
int col_intersect_line(vec2 pos0, vec2 pos1, vec2 *out_collision, vec2 *out_before_collision)
{
	float d = distance(pos0, pos1);
	vec2 last = pos0;
	
	for(float f = 0; f < d; f++)
	{
		float a = f/d;
		vec2 pos = mix(pos0, pos1, a);
		if(col_is_solid(round(pos.x), round(pos.y)))
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

