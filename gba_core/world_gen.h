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
#define TILE_GOLD 28
#define TILE_DOOR_OPEN_T 32
#define TILE_DOOR_OPEN_M 33
#define TILE_DOOR_OPEN_B 34
#define TILE_DOOR_SLAB_T 35
#define TILE_DOOR_SLAB_M 36
#define TILE_DOOR_SLAB_B 37
#define TILE_ANVIL_L 38
#define TILE_ANVIL_R 39
#define TILE_CHAIR_T 40
#define TILE_CHAIR_B 41
#define TILE_TABLE_TL 42
#define TILE_TABLE_TM 43
#define TILE_TABLE_TR 44
#define TILE_TABLE_BL 45
#define TILE_TABLE_BM 46
#define TILE_TABLE_BR 47
#define TILE_FOLIAGE_START 48
#define TILE_FOLIAGE_END   121
#define ITEM_GEL    102
#define ITEM_TORCH  103
#define ITEM_COPPER_ORE 104
#define ITEM_IRON_ORE 105
#define ITEM_COPPER_BAR 106
#define ITEM_IRON_BAR 107
#define ITEM_GOLD_ORE 125
#define ITEM_GOLD_BAR 126
#define ITEM_DOOR 127
#define ITEM_ANVIL 128
#define ITEM_CHAIR 129
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
#define ITEM_TABLE 130

// The world map array, sits in EWRAM (256KB)
extern unsigned short world_map[WORLD_H][WORLD_W];
// extern unsigned char light_map[WORLD_H][WORLD_W]; (Unused)

// Random seed
extern unsigned int seed;
unsigned int rand_next();

// Functions
void generate_world(void);
void grow_tree(int x, int y, int sync);
void set_tile(int x, int y, unsigned short tile);

#endif
