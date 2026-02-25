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
// #define TILE_LAVA   12 (Unused)
#define TILE_FURNACE 13   // TL
#define TILE_WORKBENCH 14 // L
#define TILE_CHEST 15     // TL
#define TILE_MUD   16
#define TILE_JUNGLE_GRASS 17
#define TILE_FURNACE_TM 18
#define TILE_FURNACE_TR 19
#define TILE_FURNACE_BL 20
#define TILE_FURNACE_BM 21
#define TILE_FURNACE_BR 22
#define TILE_WORKBENCH_R 23
#define TILE_CHEST_TR 24
#define TILE_CHEST_BL 25
#define TILE_CHEST_BR 26
#define TILE_SAPLING 27
#define ITEM_GEL    102
#define ITEM_TORCH  103
#define ITEM_COPPER_ORE 104
#define ITEM_IRON_ORE 105
#define ITEM_COPPER_BAR 106
#define ITEM_IRON_BAR 107
#define ITEM_WORKBENCH 108
#define ITEM_FURNACE 109
#define ITEM_CHEST 110
#define ITEM_DIRT    111
#define ITEM_STONE   112
#define ITEM_ASH     113
// #define ITEM_WOOD    114 (Unused)
#define ITEM_PLANKS  115
#define ITEM_MUD     116
#define ITEM_JUNGLE_GRASS 117
#define ITEM_ACORN   118
#define ITEM_AXE     119
#define ITEM_REGEN_BAND 120
#define ITEM_MAGIC_MIRROR 121
#define ITEM_CLOUD_BOTTLE 122
#define ITEM_DEPTH_METER 123
#define ITEM_ROCKET_BOOTS 124

// The world map array, sits in EWRAM (256KB)
extern unsigned char world_map[WORLD_H][WORLD_W];
// extern unsigned char light_map[WORLD_H][WORLD_W]; (Unused)

// Random seed
extern unsigned int seed;
unsigned int rand_next();

// Functions
void generate_world(void);
void grow_tree(int x, int y);

#endif
