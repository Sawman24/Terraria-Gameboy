#ifndef WORLD_GEN_H
#define WORLD_GEN_H

#define WORLD_W 256
#define WORLD_H 128

// Tile IDs
#define TILE_AIR    0
#define TILE_DIRT   1
#define TILE_STONE  2
#define TILE_GRASS  3
#define TILE_WOOD   4
#define TILE_LEAVES 5
#define TILE_COPPER 6
#define TILE_IRON   7
#define TILE_PLANKS 8
#define TILE_TORCH  9
#define TILE_ASH    10
#define TILE_GRASS_PLANTS 11
#define TILE_LAVA   12
#define ITEM_GEL    102
#define ITEM_TORCH  103

// The world map array, sits in EWRAM (256KB)
extern unsigned char world_map[WORLD_H][WORLD_W];
extern unsigned char light_map[WORLD_H][WORLD_W];

// Random seed
extern unsigned int seed;

// Functions
void generate_world(void);

#endif
