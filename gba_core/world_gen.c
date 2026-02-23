#include "world_gen.h"

unsigned char __attribute__((section(".ewram"))) world_map[WORLD_H][WORLD_W];
unsigned char __attribute__((section(".ewram"))) light_map[WORLD_H][WORLD_W];

int jungle_x = 0;
int jungle_y = 0;

unsigned int seed = 12345;

static unsigned int rand_next() {
    seed = seed * 1103515245 + 12345;
    return (seed >> 16) & 0x7FFF;
}

// Simple 1D smoothing
static void smooth_heightmap(int* heights) {
    int temp[WORLD_W];
    for (int i = 0; i < WORLD_W; i++) {
        int left = heights[(i > 0) ? (i - 1) : 0];
        int right = heights[(i < WORLD_W - 1) ? (i + 1) : (WORLD_W - 1)];
        int mid = heights[i];
        temp[i] = (left + mid * 2 + right) / 4;
    }
    for (int i = 0; i < WORLD_W; i++) {
        heights[i] = temp[i];
    }
}

// Cellular Automata for Cave Generation
static void generate_caves() {
    // We do a small version to keep it fast on GBA
    // We only process underground tiles
    
    // Pass 1: Random noise (0 or 1)
    for (int y = 35; y < WORLD_H - 10; y++) {
        for (int x = 5; x < WORLD_W - 5; x++) {
            // Chance to be a cave (air): 48%
            if (world_map[y][x] == TILE_STONE && (rand_next() % 100) < 48) {
                world_map[y][x] = TILE_AIR;
            }
        }
    }
    
    // Pass 2/3: Cellular Automata smoothing
    unsigned char temp[10]; // only need temp buffer per column to not mess up reads, but 
    // actually doing it perfectly requires a full temp map. To save RAM, we can just do in-place
    // which creates directional artifacts, or use a small rolling buffer.
    // Rolling buffer approach:
    unsigned char row_above[WORLD_W];
    unsigned char row_cur[WORLD_W];
    
    for (int pass = 0; pass < 3; pass++) {
        // Init buffers
        for (int x=0; x<WORLD_W; x++) row_above[x] = world_map[30-1][x];
        for (int x=0; x<WORLD_W; x++) row_cur[x] = world_map[30][x];
        
        for (int y = 30; y < WORLD_H - 2; y++) {
            unsigned char row_next[WORLD_W];
            for (int x=0; x<WORLD_W; x++) row_next[x] = world_map[y+1][x];
            
            for (int x = 1; x < WORLD_W - 1; x++) {
                // Count wall neighbors in 3x3
                int walls = 0;
                if (row_above[x-1] != TILE_AIR) walls++;
                if (row_above[x] != TILE_AIR) walls++;
                if (row_above[x+1] != TILE_AIR) walls++;
                if (row_cur[x-1] != TILE_AIR) walls++;
                if (row_cur[x+1] != TILE_AIR) walls++;
                if (row_next[x-1] != TILE_AIR) walls++;
                if (row_next[x] != TILE_AIR) walls++;
                if (row_next[x+1] != TILE_AIR) walls++;
                
                if (walls >= 4) {
                    world_map[y][x] = TILE_STONE; // Fill
                } else {
                    world_map[y][x] = TILE_AIR;   // Empty
                }
            }
            // shift down
            for (int x=0; x<WORLD_W; x++) row_above[x] = row_cur[x];
            for (int x=0; x<WORLD_W; x++) row_cur[x] = world_map[y][x];
    }
}
}

static void generate_worms() {
    int num_worms = 12 + (rand_next() % 8);
    for (int w = 0; w < num_worms; w++) {
        int cur_x = 10 + (rand_next() % (WORLD_W - 20));
        int cur_y = 30 + (rand_next() % (WORLD_H - 50));
        int len = 40 + (rand_next() % 60);
        for (int i = 0; i < len; i++) {
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    int nx = cur_x + dx; int ny = cur_y + dy;
                    if (nx >= 2 && nx < WORLD_W - 2 && ny >= 2 && ny < WORLD_H - 2)
                        world_map[ny][nx] = TILE_AIR;
                }
            }
            cur_x += (rand_next() % 3) - 1; cur_y += (rand_next() % 3) - 1;
            if ((rand_next() % 10) < 3) cur_y++;
            if (cur_x < 5) cur_x = 5; if (cur_x > WORLD_W - 5) cur_x = WORLD_W - 5;
        }
    }
}

static void generate_foliage() {
    for (int y = 1; y < WORLD_H - 1; y++) {
        for (int x = 1; x < WORLD_W - 1; x++) {
            if (world_map[y][x] == TILE_AIR) {
                unsigned char below = world_map[y+1][x];
                if (below == TILE_GRASS) {
                    if ((rand_next() % 100) < 15) world_map[y][x] = TILE_GRASS_PLANTS;
                }
            }
        }
    }
}

static void generate_cave_entrances(int* heights) {
    int num_entrances = 1 + (rand_next() % 2); // 1 to 2 entrances
    for (int e = 0; e < num_entrances; e++) {
        int cx = 30 + (rand_next() % (WORLD_W - 60));
        int cy = heights[cx];
        
        // Diagonal snaking path
        int x_dir = ((rand_next() % 100) < 50) ? -1 : 1;
        for (int y = cy; y < 45; y++) {
            if ((rand_next() % 100) < 20) x_dir = (rand_next() % 3) - 1; // drift
            cx += x_dir;
            
            // Carve a 4x2 pocket
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int nx = cx + dx;
                    int ny = y + dy;
                    if (nx >= 2 && nx < WORLD_W - 2 && ny >= 0 && ny < WORLD_H) {
                        world_map[ny][nx] = TILE_AIR;
                    }
                }
            }
        }
    }
}

// Generate simple trees on surface
static void generate_trees(int* heights) {
    for (int x = 3; x < WORLD_W - 3; x++) {
        int y = heights[x];
        // Ensure flat ground
        if (world_map[y][x] == TILE_GRASS && 
            heights[x-1] == y && heights[x+1] == y) {
            
            // 15% chance to place tree
            if ((rand_next() % 100) < 15) {
                int height = 5 + (rand_next() % 8); // 5 to 12 tiles tall
                // Trunk
                for (int ty = 0; ty < height; ty++) {
                    world_map[y - 1 - ty][x] = TILE_WOOD;
                }
                
                // Branches (Tiles 84-107)
                // Style 0: L=84, R=88. Style 1: L=92, R=96. Style 2: L=100, R=104.
                // Each is 2x2 tiles. 
                // We'll place branches between ground (ty=1) and below canopy top (ty < height-3)
                for (int ty = 1; ty < height - 3; ty++) {
                    if ((rand_next() % 100) < 25) { // 25% chance of branch at this height
                        int side = (rand_next() % 100) < 50 ? -1 : 1; // -1=Left, 1=Right
                        int style = rand_next() % 3;
                        int branch_base = 87 + (style * 8) + (side == 1 ? 4 : 0);
                        
                        int start_bx = (side == -1) ? (x - 2) : (x + 1);
                        int start_by = y - 1 - ty - 1; // 2x2 tiles, anchor so branch looks like it's coming from trunk
                        
                        for (int bcy = 0; bcy < 2; bcy++) {
                            for (int bcx = 0; bcx < 2; bcx++) {
                                int map_bx = start_bx + bcx;
                                int map_by = start_by + bcy;
                                if (map_bx >= 0 && map_bx < WORLD_W && map_by >= 0 && map_by < WORLD_H) {
                                    if (world_map[map_by][map_bx] == TILE_AIR) {
                                        world_map[map_by][map_bx] = branch_base + (bcy * 2) + bcx;
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Leaves (5x5 Canopy from Tiles 9 onwards)
                // Tree Tops: 0 (tiles 12-36), 1 (tiles 37-61), 2 (tiles 62-86)
                int tree_type = rand_next() % 3;
                int base_tile = 12 + (tree_type * 25);
                
                // Canopy overlaps the top 2 trunk blocks
                // Tree top block is at `y - height`
                int trunk_top_y = y - height;
                
                // Canopy center X = 2 (out of 5 width)
                // Canopy center/bottom overlap Y = 3 (out of 5 height)
                int start_x = x - 2;
                int start_y = trunk_top_y - 3; // Center the canopy on the topmost trunk block
                
                for (int cy = 0; cy < 5; cy++) {
                    for (int cx = 0; cx < 5; cx++) {
                        int map_x = start_x + cx;
                        int map_y = start_y + cy;
                        int tile_index = base_tile + (cy * 5) + cx;
                        
                        if (map_x >= 0 && map_x < WORLD_W && map_y >= 0 && map_y < WORLD_H) {
                            // Only place canopy if we're placing over AIR, or replacing the trunk top.
                            if (world_map[map_y][map_x] == TILE_AIR || world_map[map_y][map_x] == TILE_WOOD) {
                                world_map[map_y][map_x] = tile_index;
                            }
                        }
                    }
                }
                
                // Skip ahead to not overlap trees too closely
                x += 4 + (rand_next() % 3);
            }
        }
    }
}

static void spawn_ore_vein(int x, int y, unsigned char ore_type, int size) {
    for (int i = 0; i < size; i++) {
        if (x >= 0 && x < WORLD_W && y >= 0 && y < WORLD_H && world_map[y][x] == TILE_STONE) {
            world_map[y][x] = ore_type;
        }
        // Randomly wander to next contiguous block
        int r = rand_next() % 4;
        if (r == 0) x++;
        else if (r == 1) x--;
        else if (r == 2) y++;
        else y--;
    }
}

static void generate_ores() {
    for (int y = 31; y < WORLD_H; y++) {
        for (int x = 0; x < WORLD_W; x++) {
            if (world_map[y][x] == TILE_STONE || world_map[y][x] == TILE_ASH) {
                int p = rand_next() % 1000;
                
                // Underground Ores (31-70)
                if (y <= 70) {
                    if (p < 5) spawn_ore_vein(x, y, TILE_COPPER, 4 + (rand_next() % 6));
                    else if (p < 8) spawn_ore_vein(x, y, TILE_IRON, 3 + (rand_next() % 5));
                } 
                // Cavern Ores (71-110)
                else if (y <= 110) {
                    if (p < 6) spawn_ore_vein(x, y, TILE_IRON, 5 + (rand_next() % 8));
                    // Add more rare ores here if desired
                }
                // Underworld Ores (111-127)
                else {
                    // Placeholder for Hellstone (maybe reusing Copper or Iron ID for now)
                    if (p < 10) spawn_ore_vein(x, y, TILE_IRON, 6 + (rand_next() % 10));
                }
            }
        }
    }
}

void generate_world(void) {
    // 1. Generate surface heightmap (target 0-30 range)
    int heights[WORLD_W];
    int current_y = 25; // Average start depth
    for (int x = 0; x < WORLD_W; x++) {
        heights[x] = current_y;
        if ((rand_next() % 100) < 35) {
            current_y += (rand_next() % 3) - 1;
        }
        if (current_y < 15) current_y = 15;   // Hills never above tile 15
        if (current_y > 30) current_y = 30;   //Valleys never below tile 30
    }
    
    smooth_heightmap(heights);
    smooth_heightmap(heights);

    // 2. Fill layers
    for (int x = 0; x < WORLD_W; x++) {
        int surface = heights[x];
        for (int y = 0; y < WORLD_H; y++) {
            if (y <= 30) { // Layer 1: Space/Surface
                if (y < surface) {
                    world_map[y][x] = TILE_AIR;
                } else if (y == surface) {
                    world_map[y][x] = TILE_GRASS;
                } else {
                    world_map[y][x] = TILE_DIRT;
                }
            } else if (y <= 70) { // Layer 2: Underground
                // Gradually transition from Dirt to Stone
                int stone_chance = (y - 30) * 2; // Linear increase
                if ((rand_next() % 100) < stone_chance) {
                    world_map[y][x] = TILE_STONE;
                } else {
                    world_map[y][x] = TILE_DIRT;
                }
            } else if (y <= 110) { // Layer 3: Caverns
                world_map[y][x] = TILE_STONE;
            } else { // Layer 4: Underworld (111-127)
                // Bottom of the world is Ash
                world_map[y][x] = TILE_ASH;
            }
        }
    }

    // 3. Bedrock
    for (int x = 0; x < WORLD_W; x++) {
        world_map[WORLD_H - 1][x] = TILE_STONE; 
    }

    // 4. Underworld Cavern (Jagged Air Strip 115-122)
    int cave_y = 115;
    int cave_h = 5;
    for (int x = 0; x < WORLD_W; x++) {
        // Shift the cave position and height slightly for jaggedness
        if ((rand_next() % 100) < 40) cave_y += (rand_next() % 3) - 1;
        if ((rand_next() % 100) < 40) cave_h += (rand_next() % 3) - 1;
        
        // Clamp values to keep it within the band (112 - 125)
        if (cave_y < 112) cave_y = 112;
        if (cave_y > 118) cave_y = 118;
        if (cave_h < 4) cave_h = 4;
        if (cave_h > 8) cave_h = 8;

        for (int y = cave_y; y < cave_y + cave_h; y++) {
            if (y < WORLD_H - 1) { // Don't break bedrock
                world_map[y][x] = TILE_AIR;
            }
        }
    }

    // 5. Caves (targeting Underground and Caverns)
    generate_caves();
    generate_worms(); 
    generate_cave_entrances(heights);
    
    // 6. Features
    generate_ores();
    generate_trees(heights);
    generate_foliage();
    
    // 7. Underworld Detail (Lava pockets and pools)
    for (int x = 0; x < WORLD_W; x++) {
        // Add lava pools at the very bottom of the air cavern
        for (int y = 120; y < WORLD_H - 1; y++) {
            if (world_map[y][x] == TILE_AIR && (rand_next() % 100) < 20) {
                world_map[y][x] = TILE_LAVA;
            }
            // Occasional lava pockets in the ash
            if (world_map[y][x] == TILE_ASH && (rand_next() % 100) < 5) {
                world_map[y][x] = TILE_LAVA;
            }
        }
    }
}
