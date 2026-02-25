#include <stdlib.h>
#include "world_gen.h"

unsigned char __attribute__((section(".ewram"))) world_map[WORLD_H][WORLD_W];
// unsigned char __attribute__((section(".ewram"))) light_map[WORLD_H][WORLD_W]; // Unused

// int jungle_x = 0; // Unused
// int jungle_y = 0; // Unused

unsigned int seed = 12345;

unsigned int rand_next() {
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
// Cellular Automata for Cave Generation
static void generate_caves() {
    // Pass 1: Random noise (Jittered chance per area for variation)
    for (int y = 45; y < WORLD_H - 10; y++) {
        int base_chance = 45 + (y % 10); // Jitter by depth
        for (int x = 5; x < WORLD_W - 5; x++) {
            int chance = base_chance;
            if ((x % 64) < 20) chance += 10; // "Dense" columns
            if ((x % 64) > 44) chance -= 10; // "Sparse" columns

            if (world_map[y][x] == TILE_STONE && (rand_next() % 100) < chance) {
                world_map[y][x] = TILE_AIR;
            }
        }
    }
    
    // Pass 2/3: Cellular Automata smoothing
    unsigned char row_above[WORLD_W];
    unsigned char row_cur[WORLD_W];
    
    for (int pass = 0; pass < 3; pass++) {
        for (int x = 0; x < WORLD_W; x++) row_above[x] = world_map[44][x];
        for (int x = 0; x < WORLD_W; x++) row_cur[x] = world_map[45][x];
        
        for (int y = 45; y < WORLD_H - 2; y++) {
            unsigned char row_next[WORLD_W];
            for (int x = 0; x < WORLD_W; x++) row_next[x] = world_map[y+1][x];
            
            for (int x = 1; x < WORLD_W - 1; x++) {
                int walls = 0;
                if (row_above[x-1] != TILE_AIR) walls++;
                if (row_above[x] != TILE_AIR) walls++;
                if (row_above[x+1] != TILE_AIR) walls++;
                if (row_cur[x-1] != TILE_AIR) walls++;
                if (row_cur[x+1] != TILE_AIR) walls++;
                if (row_next[x-1] != TILE_AIR) walls++;
                if (row_next[x] != TILE_AIR) walls++;
                if (row_next[x+1] != TILE_AIR) walls++;
                
                // Varied threshold for pass 3 creates more "cragginess"
                int threshold = (pass == 2) ? (4 + (rand_next() % 2)) : 4;

                if (walls >= threshold) {
                    if (y >= 111) world_map[y][x] = TILE_ASH;
                    else world_map[y][x] = TILE_STONE;
                } else {
                    world_map[y][x] = TILE_AIR;
                }
            }
            for (int x = 0; x < WORLD_W; x++) row_above[x] = row_cur[x];
            for (int x = 0; x < WORLD_W; x++) row_cur[x] = world_map[y][x];
        }
    }
}

static void generate_worms() {
    int num_worms = 15 + (rand_next() % 10);
    for (int w = 0; w < num_worms; w++) {
        int cur_x = 10 + (rand_next() % (WORLD_W - 20));
        int cur_y = 35 + (rand_next() % (WORLD_H - 60));
        int type = rand_next() % 3; // 0=classic, 1=fat/room, 2=vertical
        
        int len = 30 + (rand_next() % 50);
        int w_size = (type == 1) ? 3 : 2;
        
        for (int i = 0; i < len; i++) {
            // Carve the segment
            for (int dy = 0; dy < w_size; dy++) {
                for (int dx = 0; dx < w_size; dx++) {
                    int nx = cur_x + dx; int ny = cur_y + dy;
                    if (nx >= 2 && nx < WORLD_W - 2 && ny >= 2 && ny < WORLD_H - 2)
                        world_map[ny][nx] = TILE_AIR;
                }
            }
            
            // Movement logic
            if (type == 2) { // Vertical heavy
                if ((rand_next() % 100) < 20) cur_x += (rand_next() % 3) - 1;
                cur_y += 1;
            } else { // Classic and Fat
                cur_x += (rand_next() % 3) - 1;
                cur_y += (rand_next() % 3) - 1;
                if ((rand_next() % 10) < 2) cur_y++; // slight downward bias
            }
            
            if (cur_x < 5 || cur_x > WORLD_W-5 || cur_y > WORLD_H-5) break;
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
        int cx = 20 + (rand_next() % (WORLD_W - 40));
        int cy = heights[cx];
        
        int type = rand_next() % 3; // 0=Tunnel, 1=Shaft, 2=Wide Mouth
        int depth = 15 + (rand_next() % 25);
        int width = 2 + (rand_next() % 2);
        
        if (type == 0) { // Sloped Tunnel
            int x_dir = ((rand_next() % 100) < 50) ? -1 : 1;
            for (int y = cy; y < cy + depth; y++) {
                if ((rand_next() % 100) < 30) x_dir = (rand_next() % 3) - 1;
                cx += x_dir;
                
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = -width; dx <= width; dx++) {
                        int nx = cx + dx, ny = y + dy;
                        if (nx >= 2 && nx < WORLD_W - 2 && ny >= 0 && ny < WORLD_H)
                            world_map[ny][nx] = TILE_AIR;
                    }
                }
            }
        } else if (type == 1) { // Vertical Shaft
            int start_width = 1 + (rand_next() % 2);
            for (int y = cy; y < cy + depth + 10; y++) {
                if ((rand_next() % 100) < 15) cx += (rand_next() % 3) - 1; // minor drift
                for (int dx = -start_width; dx <= start_width; dx++) {
                    int nx = cx + dx;
                    if (nx >= 2 && nx < WORLD_W - 2 && y >= 0 && y < WORLD_H)
                        world_map[y][nx] = TILE_AIR;
                }
            }
        } else { // Wide Mouth / Cavern
            int mouth_w = 4 + (rand_next() % 4);
            int mouth_h = 3 + (rand_next() % 3);
            for (int y = cy - 2; y < cy + mouth_h; y++) {
                int cur_w = mouth_w - (abs(y - cy));
                if (cur_w < 1) cur_w = 1;
                for (int dx = -cur_w; dx <= cur_w; dx++) {
                    int nx = cx + dx;
                    if (nx >= 2 && nx < WORLD_W - 2 && y >= 0 && y < WORLD_H)
                        world_map[y][nx] = TILE_AIR;
                }
            }
            // Add a small worm at the bottom of the mouth
            int cur_x = cx;
            int cur_y = cy + mouth_h;
            for(int i=0; i<20; i++) {
                for(int dy=-1; dy<=1; dy++) {
                    for(int dx=-1; dx<=1; dx++) {
                        int nx = cur_x+dx, ny = cur_y+dy;
                        if(nx>=2 && nx<WORLD_W-2 && ny>=2 && ny<WORLD_H-2) world_map[ny][nx] = TILE_AIR;
                    }
                }
                cur_x += (rand_next()%3)-1; cur_y += (rand_next()%2);
            }
        }
    }
}

// Grow a mature tree at (x,y) where (x,y) is ground level (the block replaced is y-1)
void grow_tree(int x, int y) {
    if (x < 3 || x >= WORLD_W - 3 || y < 15 || y >= WORLD_H) return;

    int height = 5 + (rand_next() % 8); // 5 to 12 tiles tall
    
    // Check if space above is clear (up to height + canopy overhead)
    for (int ty = 1; ty < height + 5; ty++) {
        if (y - ty < 0) return;
        if (world_map[y - ty][x] != TILE_AIR && world_map[y - ty][x] != TILE_SAPLING) return;
    }

    // Trunk
    for (int ty = 0; ty < height; ty++) {
        world_map[y - 1 - ty][x] = TILE_WOOD;
    }
    
    // Branches
    for (int ty = 1; ty < height - 3; ty++) {
        if ((rand_next() % 100) < 25) { // 25% chance of branch at this height
            int side = (rand_next() % 100) < 50 ? -1 : 1; 
            int style = rand_next() % 3;
            int branch_base = 103 + (style * 8) + (side == 1 ? 4 : 0);
            
            int start_bx = (side == -1) ? (x - 2) : (x + 1);
            int start_by = y - 1 - ty - 1; 
            
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
    
    // Leaves (5x5 Canopy)
    int tree_type = rand_next() % 3;
    int base_tile = 28 + (tree_type * 25);
    int trunk_top_y = y - height;
    int start_x = x - 2;
    int start_y = trunk_top_y - 3; 
    
    for (int cy = 0; cy < 5; cy++) {
        for (int cx = 0; cx < 5; cx++) {
            int map_x = start_x + cx;
            int map_y = start_y + cy;
            int tile_index = base_tile + (cy * 5) + cx;
            if (map_x >= 0 && map_x < WORLD_W && map_y >= 0 && map_y < WORLD_H) {
                if (world_map[map_y][map_x] == TILE_AIR || world_map[map_y][map_x] == TILE_WOOD) {
                    world_map[map_y][map_x] = tile_index;
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
                grow_tree(x, y);
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

static void generate_chests() {
    int chests_spawned = 0;
    int target_chests = 4 + (rand_next() % 3); // 4 to 6
    int attempts = 0;
    while (chests_spawned < target_chests && attempts < 1000) {
        attempts++;
        int rx = 10 + (rand_next() % (WORLD_W - 20));
        int ry = 45 + (rand_next() % (WORLD_H - 10)); // Underground & Caverns
        
        // Find ground
        while (ry < WORLD_H - 3 && world_map[ry][rx] == TILE_AIR) ry++;
        
        // Spot must be air, and ground must be solid
        ry--; // ry is now the last air block
        if (ry > 45 && rx < WORLD_W - 2 &&
            world_map[ry][rx] == TILE_AIR && world_map[ry][rx+1] == TILE_AIR &&
            world_map[ry-1][rx] == TILE_AIR && world_map[ry-1][rx+1] == TILE_AIR &&
            world_map[ry+1][rx] != TILE_AIR && world_map[ry+1][rx+1] != TILE_AIR) {
            
            // Place 2x2 chest: TL=TILE_CHEST, TR=TILE_CHEST_TR, BL=TILE_CHEST_BL, BR=TILE_CHEST_BR
            world_map[ry-1][rx] = TILE_CHEST;
            world_map[ry-1][rx+1] = TILE_CHEST_TR;
            world_map[ry][rx] = TILE_CHEST_BL;
            world_map[ry][rx+1] = TILE_CHEST_BR;
            chests_spawned++;
        }
    }
}

static void generate_ores() {
    for (int y = 45; y < WORLD_H; y++) {
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

    smooth_heightmap(heights);
    smooth_heightmap(heights);

    // 2. Fill layers
    for (int x = 0; x < WORLD_W; x++) {
        int surface = heights[x];
        // Stone starts between 3 and 7 blocks below the surface
        int stone_start = surface + 3 + (rand_next() % 5); 
        
        for (int y = 0; y < WORLD_H; y++) {
            if (y < surface) {
                world_map[y][x] = TILE_AIR;
            } else if (y == surface) {
                world_map[y][x] = TILE_GRASS;
            } else if (y < stone_start) {
                world_map[y][x] = TILE_DIRT;
            } else if (y <= 110) { // Stone layer (Caverns)
                // Gradually transition from Dirt to Stone just after the stone_start
                if (y == stone_start) {
                    // Small chance for a few dirt pockets early in the stone
                    if ((rand_next() % 100) < 20) world_map[y][x] = TILE_DIRT;
                    else world_map[y][x] = TILE_STONE;
                } else {
                    world_map[y][x] = TILE_STONE;
                }
            } else { // Layer 4: Underworld (111-127)
                // Jagged transition to Underworld starts around 107 (already handled in CA but good for initial fill)
                world_map[y][x] = TILE_ASH;
            }
        }
    }

    // 3. Bedrock
    for (int x = 0; x < WORLD_W; x++) {
        world_map[WORLD_H - 1][x] = TILE_STONE; 
    }

    // 4. Caves (targeting Underground and Caverns)
    generate_caves();
    generate_worms(); 
    generate_cave_entrances(heights);
    generate_chests();
    
    // 6. Features
    generate_ores();
    generate_trees(heights);
    generate_foliage();
    
    // 7. Underworld Cavern (Jagged Air Strip 112-125)
    // We do this AFTER caves/smoothing so it doesn't get filled back in
    int cave_y = 118; // Center of the 112-125 band
    int cave_h = 4;   // Target average height
    for (int x = 0; x < WORLD_W; x++) {
        // Shift the cave position and height slightly for jaggedness
        if ((rand_next() % 100) < 40) cave_y += (rand_next() % 3) - 1;
        if ((rand_next() % 100) < 30) cave_h += (rand_next() % 3) - 1;
        
        // Clamp height to keep it around 4
        if (cave_h < 3) cave_h = 3;
        if (cave_h > 6) cave_h = 6;

        // Clamp values to keep it within the band (112 - 125)
        if (cave_y < 112) cave_y = 112;
        if (cave_y + cave_h > 126) cave_y = 126 - cave_h;

        for (int y = cave_y; y < cave_y + cave_h; y++) {
            if (y < WORLD_H - 1) { // Don't break bedrock
                world_map[y][x] = TILE_AIR;
            }
        }
    }

    // 8. Underworld Detail (Post-processing)
    // Removed lava generation as requested
}
