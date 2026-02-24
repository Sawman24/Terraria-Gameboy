#include <stdint.h>
#include <stdlib.h>
#include "world_gen.h"
#include "tileset.h"
#include "sprite_gfx.h"

#define ITEM_SWORD    100
#define ITEM_PICKAXE  101

#define MAX_SLIMES 8
typedef struct {
    int x, y, dx, dy;
    int hp;
    int active;
    int type; // 0=Green, 1=Cave
    int jump_timer;
    int flicker;
    int stuck_timer;
    int stuck_x;
    int stuck_y;
} Slime;

// --- Hardware Registers ---
#define REG_DISPCNT     (*(volatile uint16_t*)0x04000000)
#define REG_VCOUNT      (*(volatile uint16_t*)0x04000006)
#define REG_BG0CNT      (*(volatile uint16_t*)0x04000008)
#define REG_BG0HOFS     (*(volatile uint16_t*)0x04000010)
#define REG_BG0VOFS     (*(volatile uint16_t*)0x04000012)
#define REG_KEYINPUT    (*(volatile uint16_t*)0x04000130)

#define MODE_0          0x0000
#define BG0_ENABLE      0x0100
#define BG1_ENABLE      0x0200
#define BG2_ENABLE      0x0400

#define REG_BG1CNT      (*(volatile uint16_t*)0x0400000A)
#define REG_BG1HOFS     (*(volatile uint16_t*)0x04000014)
#define REG_BG1VOFS     (*(volatile uint16_t*)0x04000016)
#define REG_BG2CNT      (*(volatile uint16_t*)0x0400000C)
#define REG_BG2HOFS     (*(volatile uint16_t*)0x04000018)
#define REG_BG2VOFS     (*(volatile uint16_t*)0x0400001A)
#define REG_BLDCNT      (*(volatile uint16_t*)0x04000050)
#define REG_BLDALPHA    (*(volatile uint16_t*)0x04000052)
#define REG_BLDY        (*(volatile uint16_t*)0x04000054)

// Background Control
#define BG_256_COLOR    0x0080
#define BG_SIZE_32x32   0x0000
#define BG_TILE_BASE(n) ((n) << 2)
#define BG_MAP_BASE(n)  ((n) << 8)

// Object Control
#define OBJ_ENABLE      0x1000
#define OBJ_1D_MAP      0x0040

// VRAM Memory
#define MEM_PAL_BG      ((volatile uint16_t*)0x05000000)
#define MEM_PAL_OBJ     ((volatile uint16_t*)0x05000200)
#define MEM_VRAM        ((volatile uint16_t*)0x06000000)
#define MEM_BG_TILES    ((volatile uint16_t*)0x06000000)
#define MEM_OBJ_TILES   ((volatile uint16_t*)0x06010000)
#define MEM_OAM         ((volatile uint16_t*)0x07000000)

volatile uint16_t* bg_map = (volatile uint16_t*)(0x06000000 + (27 * 2048));
volatile uint16_t* bg2_map = (volatile uint16_t*)(0x06000000 + (26 * 2048));

static void fill_bg_map(int start_tile) {
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            int tx = x % 6;
            int ty = y % 6;
            bg_map[y * 32 + x] = start_tile + (ty * 6) + tx;
        }
    }
}

static void draw_clouds() {
    // Cloud 0: 8x4 tiles, start index 192
    // Cloud 1: 6x2 tiles, start index 224
    // Cloud 2: 5x3 tiles, start index 236
    
    // Cloud 0 at (2, 2)
    for(int ty=0; ty<4; ty++) {
        for(int tx=0; tx<8; tx++) {
            bg_map[(2 + ty) * 32 + (2 + tx)] = 192 + (ty * 8) + tx;
        }
    }
    
    // Cloud 1 at (15, 6)
    for(int ty=0; ty<2; ty++) {
        for(int tx=0; tx<6; tx++) {
            bg_map[(6 + ty) * 32 + (15 + tx)] = 224 + (ty * 6) + tx;
        }
    }
    
    // Cloud 2 at (24, 3)
    for(int ty=0; ty<3; ty++) {
        for(int tx=0; tx<5; tx++) {
            bg_map[(3 + ty) * 32 + (24 + tx)] = 236 + (ty * 5) + tx;
        }
    }
}

typedef struct {
    uint16_t attr0;
    uint16_t attr1;
    uint16_t attr2;
    uint16_t fill;
} OBJ_ATTR;

// Keyboard
#define KEY_A           0x0001
#define KEY_B           0x0002
#define KEY_SELECT      0x0004
#define KEY_START       0x0008
#define KEY_RIGHT       0x0010
#define KEY_LEFT        0x0020
#define KEY_UP          0x0040
#define KEY_DOWN_BTN    0x0080
#define KEY_R           0x0100
#define KEY_L           0x0200

uint16_t prev_keys = 0;
uint16_t curr_keys = 0;
#define KEY_DOWN(k)     (~curr_keys & (k))
#define KEY_PRESSED(k)  ((~curr_keys & (k)) && (prev_keys & (k)))

// DMA
#define REG_DMA3SAD     (*(volatile uint32_t*)0x040000D4)
#define REG_DMA3DAD     (*(volatile uint32_t*)0x040000D8)
#define REG_DMA3CNT     (*(volatile uint32_t*)0x040000DC)
#define DMA_ENABLE      (1u << 31)
#define DMA_16BIT       (0u << 26)
#define DMA_32BIT       (1u << 26)

#include "title_screen.c"

static inline void vsync(void) {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

// Camera coordinates (pixel units)
int cam_x = 0;
int cam_y = 0;

// The hardware map array in VRAM (32x32 tiles, block=2KB)
// Screen Base Block 28 = 28 * 2048 bytes = 56KB offset
volatile uint16_t* hw_map = MEM_VRAM + (28 * 2048 / 2);

void update_map_seam(int prev_cx, int prev_cy, int cx, int cy) {
    int tx_start = cx / 8;
    int ty_start = cy / 8;
    
    // Redraw the active screen area (around 32x22 tiles)
    for (int y = -2; y < 22; y++) {
        for (int x = -1; x < 32; x++) {
            int map_y = ty_start + y;
            int map_x = tx_start + x;
            if (map_x >= 0 && map_x < WORLD_W && map_y >= 0 && map_y < WORLD_H) {
                int hw_y = map_y & 31;
                int hw_x = map_x & 31;
                hw_map[hw_y * 32 + hw_x] = world_map[map_y][map_x];
            }
        }
    }
}

int is_solid(int px, int py) {
    if (px < 0 || py < 0 || px >= WORLD_W*8 || py >= WORLD_H*8) return 1;
    int tx = px / 8;
    int ty = py / 8;
    int tile = world_map[ty][tx];
    // Solid Blocks: Air(0), Dirt(1), Stone(2), Grass(3), Wood(4), Leaves(5), Copper(6), Iron(7), Planks(8), Torch(9), Ash(10), Mud(16), JungleGrass(17)
    if (tile == TILE_AIR || tile == TILE_WOOD || tile == TILE_LEAVES || tile == TILE_TORCH) return 0;
    if (tile >= 11 && tile <= 15) return 0; // GrassPlants, Lava, Furnace, Workbench, Chest
    if (tile >= 18 && tile <= 250) return 0; // Empty, Sapling, Trees, Walls, UI
    return 1;
}

int check_collision(int x, int y) {
    // Check 4 corners of player hitbox
    // Box: x+10 to x+21, y+7 to y+29
    int left = x + 10;
    int right = x + 21;
    int top = y + 7;
    int bottom = y + 29; // true bottom of feet
    
    // Check multiple points along sides to avoid clipping through blocks
    if (is_solid(left, top)) return 1;
    if (is_solid(right, top)) return 1;
    if (is_solid(left, bottom)) return 1;
    if (is_solid(right, bottom)) return 1;
    
    if (is_solid(left, y + 15)) return 1;
    if (is_solid(right, y + 15)) return 1;
    if (is_solid(left, y + 22)) return 1;
    if (is_solid(right, y + 22)) return 1;
    
    return 0;
}

int main(void) {
    // 0. Show Title Screen
    run_title_screen();

    // Basic setup
    REG_DISPCNT = MODE_0 | BG0_ENABLE | BG1_ENABLE | OBJ_ENABLE | OBJ_1D_MAP;
    
    // BG0: Foreground Map
    REG_BG0CNT  = BG_256_COLOR | BG_TILE_BASE(0) | BG_MAP_BASE(28) | 0; // Priority 0 (Top)
    // BG1: Background Map (Sky/Cave Walls)
    REG_BG1CNT  = BG_256_COLOR | BG_TILE_BASE(0) | BG_MAP_BASE(27) | 1; // Priority 1
    
    // Disable Color Effects
    REG_BLDCNT = 0;
    REG_BLDALPHA = 0;
    
    // Load Palette
    for (int i = 0; i < 256; i++) {
        MEM_PAL_BG[i] = tileset_palette[i];
    }
    // Backdrop color is always index 0
    ((volatile uint16_t*)0x05000000)[0] = tileset_palette[0];
    // Load Tiles
    const uint16_t* src_gfx = (const uint16_t*)tileset_graphics;
    for (int i = 0; i < sizeof(tileset_graphics)/2; i++) {
        MEM_BG_TILES[i] = src_gfx[i];
    }
    
    fill_bg_map(120); // Start with BG2 (Dirt Walls)
    
    // Clear map
    for (int i=0; i<32*32; i++) {
        hw_map[i] = TILE_AIR;
    }

    // 1. Generate the world into EWRAM
    generate_world();
    
    // Enable Objects
    REG_DISPCNT |= OBJ_ENABLE | OBJ_1D_MAP;

    // Load Sprite Palette
    for (int i = 0; i < 256; i++) {
        MEM_PAL_OBJ[i] = sprite_palette[i];
    }
    // Load Sprite Graphics
    const uint16_t* src_pgfx = (const uint16_t*)sprite_graphics;
    for (int i = 0; i < sizeof(sprite_graphics)/2; i++) {
        MEM_OBJ_TILES[i] = src_pgfx[i];
    }
    
    // Player physics state
    // x and y are in subpixels (fixed point 256)
    int p_x = ((WORLD_W * 8) / 2) << 8;
    int p_y = 0;
    
    // Find surface
    for(int y=0; y<WORLD_H; y++){
        if(world_map[y][WORLD_W/2] != TILE_AIR) {
            p_y = ((y * 8) - 40) << 8;
            break;
        }
    }
    
    int p_dx = 0;
    int p_dy = 0;
    int grounded = 0;
    int facing_left = 0;
    int frame_timer = 0;
    int current_frame = 0;
    
    int mining_mode = 0;
    int cursor_dx = 0;
    int cursor_dy = 0;
    
    int inv_open = 0;
    int inv_mode = 0; // 0=Inventory, 1=Crafting
    int inv_cursor = 0; // 0-11 for inv, 0-2 for craft
    int inv_held_cursor = -1; // -1 = none
    int cursor_move_delay = 0;
    
    int active_slot = 0; // 0 = Slot 1, 1 = Slot 2, ...
    int hotbar_id[12] = {ITEM_SWORD, ITEM_PICKAXE, ITEM_AXE, TILE_DIRT, 0, 0, 0, 0, 0, 0, 0, 0};
    int hotbar_count[12] = {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    int swing_timer = 0;
    int swing_frame = 0;
    
    int p_health = 5;
    int p_flicker = 0;
    
    Slime slimes[MAX_SLIMES];
    for(int i=0; i<MAX_SLIMES; i++) slimes[i].active = 0;
    
    // Set camera
    cam_x = (p_x >> 8) - 120 + 16;
    cam_y = (p_y >> 8) - 80 + 16;
    
    // Init OAM to hidden
    volatile OBJ_ATTR* oam = (volatile OBJ_ATTR*)MEM_OAM;
    for(int i=0; i<128; i++) oam[i].attr0 = 0x0200; // Disable

    // Initial map draw
    update_map_seam(-999, -999, cam_x, cam_y);

    int prev_x = cam_x;
    int prev_y = cam_y;

    // 4. Main Game Loop
    while (1) {
        vsync();
        
        prev_keys = curr_keys;
        curr_keys = REG_KEYINPUT;
        seed += 7; // Update seed for randomness during gameplay
        int ix = p_x >> 8;
        int iy = p_y >> 8;
        
        if (KEY_PRESSED(KEY_SELECT) && !inv_open) {
            mining_mode = !mining_mode;
            if (mining_mode) {
                cursor_dx = 0;
                cursor_dy = 0;
            }
        }
        if (KEY_PRESSED(KEY_START)) inv_open = !inv_open;
        
        // Physics Loop
        p_dy += 32; // Gravity
        if (p_dy > 1024) p_dy = 1024; // Terminal velocity
        p_dx = 0;
        
        // Hotbar select (Universal, can change via L/R if inv closed)
        if (!inv_open) {
            if (KEY_PRESSED(KEY_R)) active_slot = (active_slot + 1) % 4;
            if (KEY_PRESSED(KEY_L)) active_slot = (active_slot + 3) % 4;
        }
        
        if (inv_open) {
            // Toggle Mode with SELECT while inside inventory
            if (KEY_PRESSED(KEY_SELECT)) {
                inv_mode = !inv_mode;
                inv_cursor = 0;
            }

            // Inventory Navigation
            if (cursor_move_delay > 0) cursor_move_delay--;
            if (cursor_move_delay == 0) {
                if (inv_mode == 0) { // Inventory Move
                    if (KEY_DOWN(KEY_RIGHT)) { inv_cursor = (inv_cursor + 1) % 12; cursor_move_delay = 10; }
                    if (KEY_DOWN(KEY_LEFT))  { inv_cursor = (inv_cursor + 11) % 12; cursor_move_delay = 10; }
                    if (KEY_DOWN(KEY_DOWN_BTN)) { inv_cursor = (inv_cursor + 4) % 12; cursor_move_delay = 10; }
                    if (KEY_DOWN(KEY_UP))    { inv_cursor = (inv_cursor + 8) % 12; cursor_move_delay = 10; }
                } else { // Crafting Move
                    if (KEY_DOWN(KEY_DOWN_BTN)) { inv_cursor = (inv_cursor + 1) % 2; cursor_move_delay = 10; }
                    if (KEY_DOWN(KEY_UP))    { inv_cursor = (inv_cursor + 1) % 2; cursor_move_delay = 10; }
                }
            }
            
            if (inv_mode == 1) { // Crafting Logic
                // ... (existing crafting logic)
                int can_craft[2] = {0,0};
                int wood_count = 0, plank_count = 0, gel_count = 0;
                for(int s=0; s<12; s++) {
                    if(hotbar_id[s] == TILE_WOOD) wood_count += hotbar_count[s];
                    if(hotbar_id[s] == TILE_PLANKS) plank_count += hotbar_count[s];
                    if(hotbar_id[s] == ITEM_GEL) gel_count += hotbar_count[s];
                }
                if(plank_count >= 1 && gel_count >= 1) can_craft[1] = 1;
                
                if (KEY_PRESSED(KEY_A) && can_craft[inv_cursor]) {
                    if (inv_cursor == 0) { 
                        // Slot 0 empty for now
                    } else if (inv_cursor == 1) { // 1 Plank + 1 Gel = 3 Torches
                        int p_left = 1, g_left = 1;
                        for(int s=0; s<12; s++) {
                            if(hotbar_id[s] == TILE_PLANKS && p_left > 0) {
                                int take = (hotbar_count[s] < p_left) ? hotbar_count[s] : p_left;
                                hotbar_count[s] -= take; p_left -= take;
                                if (hotbar_count[s] == 0) hotbar_id[s] = 0;
                            }
                            if(hotbar_id[s] == ITEM_GEL && g_left > 0) {
                                hotbar_count[s]--; g_left--;
                                if (hotbar_count[s] == 0) hotbar_id[s] = 0;
                            }
                        }
                        for(int p=0; p<3; p++) {
                            int added = 0;
                            for(int s=0; s<12; s++) {
                                if(hotbar_id[s] == ITEM_TORCH && hotbar_count[s] > 0 && hotbar_count[s] < 99) {
                                    hotbar_count[s]++; added = 1; break;
                                }
                            }
                            if(!added) {
                                for(int s=2; s<12; s++) {
                                    if(hotbar_count[s] == 0 && hotbar_id[s] != ITEM_SWORD && hotbar_id[s] != ITEM_PICKAXE) {
                                        hotbar_id[s] = ITEM_TORCH; hotbar_count[s] = 1; break;
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                // Storage Swap
                if (KEY_PRESSED(KEY_A)) {
                    if (inv_held_cursor == -1) {
                        // Pick up item (if slot not empty)
                        if (hotbar_count[inv_cursor] > 0 || inv_cursor < 3) { 
                            inv_held_cursor = inv_cursor;
                        }
                    } else {
                        // Swap or Merge
                        int tid = hotbar_id[inv_cursor];
                        int hid = hotbar_id[inv_held_cursor];
                        if (tid == hid && tid != 0 && tid != ITEM_SWORD && tid != ITEM_PICKAXE && tid != ITEM_AXE) {
                            int space = 99 - hotbar_count[inv_cursor];
                            int transfer = (hotbar_count[inv_held_cursor] < space) ? hotbar_count[inv_held_cursor] : space;
                            hotbar_count[inv_cursor] += transfer;
                            hotbar_count[inv_held_cursor] -= transfer;
                            if (hotbar_count[inv_held_cursor] == 0) {
                                hotbar_id[inv_held_cursor] = 0;
                                inv_held_cursor = -1;
                            }
                        } else {
                            int temp_id = hotbar_id[inv_cursor];
                            int temp_count = hotbar_count[inv_cursor];
                            hotbar_id[inv_cursor] = hotbar_id[inv_held_cursor];
                            hotbar_count[inv_cursor] = hotbar_count[inv_held_cursor];
                            hotbar_id[inv_held_cursor] = temp_id;
                            hotbar_count[inv_held_cursor] = temp_count;
                            inv_held_cursor = -1;
                        }
                    }
                }
            }

            p_dx = 0; // Freeze player
        } else if (mining_mode) {
            // ... (rest of mining logic remains similar but uses index 12)
            // Note: I will update the internal loops in main.c to 12 slots in the next chunk or here.
            
            // Move cursor
            if (cursor_move_delay > 0) cursor_move_delay--;
            if (cursor_move_delay == 0) {
                if (KEY_DOWN(KEY_RIGHT) && cursor_dx < 3) { cursor_dx++; cursor_move_delay = 8; }
                if (KEY_DOWN(KEY_LEFT)  && cursor_dx > -3) { cursor_dx--; cursor_move_delay = 8; }
                if (KEY_DOWN(KEY_DOWN_BTN) && cursor_dy < 3) { cursor_dy++; cursor_move_delay = 8; }
                if (KEY_DOWN(KEY_UP) && cursor_dy > -3) { cursor_dy--; cursor_move_delay = 8; }
            }
            
            // Mine/Place Block
            ix = p_x >> 8;
            iy = p_y >> 8;
            int cx = ((ix + 16) / 8) + cursor_dx;
            int cy = ((iy + 16) / 8) + cursor_dy;
            
            if (cx >= 0 && cx < WORLD_W && cy >= 0 && cy < WORLD_H) {
                if (KEY_PRESSED(KEY_B) || KEY_DOWN(KEY_B)) { 
                    int cur_id = hotbar_id[active_slot];
                    int cur_count = hotbar_count[active_slot];
                    
                    if (cur_id == ITEM_PICKAXE || cur_id == ITEM_AXE) {
                        int target = world_map[cy][cx];
                        if (target != TILE_AIR && swing_timer == 0) {
                            int drop = target;
                            if (drop == TILE_GRASS) drop = TILE_DIRT;
                            
                            int amt = 1;
                            int acorns = 0;
                            
                            if (target == TILE_WOOD) {
                                if (cur_id == ITEM_AXE) {
                                    drop = TILE_PLANKS;
                                    amt = 0;
                                    acorns = (seed % 3); // 0-2 acorns
                                    int yy = cy;
                                    while (yy >= 0) {
                                        int t_center = world_map[yy][cx];
                                        if (t_center == TILE_WOOD || t_center == TILE_LEAVES || (t_center >= 16 && t_center <= 114)) {
                                            for (int lx = cx - 2; lx <= cx + 2; lx++) {
                                                if (lx >= 0 && lx < WORLD_W) {
                                                    int t = world_map[yy][lx];
                                                    if (t == TILE_WOOD || t == TILE_LEAVES || (t >= 16 && t <= 114)) {
                                                        world_map[yy][lx] = TILE_AIR;
                                                        hw_map[(yy % 32) * 32 + (lx % 32)] = TILE_AIR;
                                                    }
                                                }
                                            }
                                            amt++;
                                            yy--;
                                        } else break;
                                    }
                                } else {
                                    amt = 0; // Pickaxe can't break trees
                                }
                            } else if (target == TILE_LEAVES || (target >= 21 && target <= 119)) {
                                amt = 0;
                            } else {
                                if (cur_id == ITEM_PICKAXE) {
                                    world_map[cy][cx] = TILE_AIR;
                                    hw_map[(cy % 32) * 32 + (cx % 32)] = TILE_AIR;
                                } else {
                                    amt = 0; // Axe can't break stone/dirt
                                }
                            }
                            
                            // Add items
                            for (int a = 0; a < amt + acorns; a++) {
                                int id = (a < amt) ? drop : ITEM_ACORN;
                                int added = 0;
                                for (int s = 0; s < 12; s++) {
                                    if (hotbar_id[s] == id && hotbar_count[s] > 0 && hotbar_count[s] < 99) {
                                        hotbar_count[s]++; added = 1; break;
                                    }
                                }
                                if (!added) {
                                    for (int s = 2; s < 12; s++) {
                                        if (hotbar_count[s] == 0 && hotbar_id[s] != ITEM_SWORD && hotbar_id[s] != ITEM_PICKAXE && hotbar_id[s] != ITEM_AXE) {
                                            hotbar_id[s] = id; hotbar_count[s] = 1; added = 1; break;
                                        }
                                    }
                                }
                            }
                            if (amt > 0 || acorns > 0 || (target != TILE_AIR && cur_id == ITEM_PICKAXE)) swing_timer = 15;
                        }
                    }
 else if (cur_id == ITEM_SWORD) {
                        if (swing_timer == 0) swing_timer = 15;
                    } else if (cur_id == ITEM_ACORN && cur_count > 0 && swing_timer == 0) {
                        // Plant Acorn
                        if (world_map[cy][cx] == TILE_AIR) {
                            unsigned char below = world_map[cy + 1][cx];
                            if (below == TILE_DIRT || below == TILE_GRASS) {
                                // Check clearance (two blocks above)
                                if (cy > 2 && world_map[cy - 1][cx] == TILE_AIR && world_map[cy - 2][cx] == TILE_AIR) {
                                    world_map[cy][cx] = TILE_SAPLING;
                                    hw_map[(cy % 32) * 32 + (cx % 32)] = TILE_SAPLING;
                                    hotbar_count[active_slot]--;
                                    if (hotbar_count[active_slot] <= 0) {
                                        hotbar_count[active_slot] = 0;
                                        hotbar_id[active_slot] = 0;
                                    }
                                    swing_timer = 15;
                                }
                            }
                        }
                    } else if (cur_id != ITEM_PICKAXE && cur_id != ITEM_AXE && cur_id != ITEM_ACORN && cur_count > 0 && swing_timer == 0 && cur_id != ITEM_GEL && cur_id != ITEM_COPPER_ORE && cur_id != ITEM_IRON_ORE && cur_id != ITEM_COPPER_BAR && cur_id != ITEM_IRON_BAR && cur_id != ITEM_WORKBENCH && cur_id != ITEM_FURNACE && cur_id != ITEM_CHEST) { // Place Block
                        if (!is_solid(cx * 8, cy * 8)) {
                            int tile_to_place = cur_id;
                            if (cur_id == ITEM_TORCH) tile_to_place = TILE_TORCH;
                            else if (cur_id == ITEM_WORKBENCH) tile_to_place = TILE_WORKBENCH;
                            else if (cur_id == ITEM_FURNACE) tile_to_place = TILE_FURNACE;
                            else if (cur_id == ITEM_CHEST) tile_to_place = TILE_CHEST;
                            
                            world_map[cy][cx] = tile_to_place;
                            hw_map[(cy % 32) * 32 + (cx % 32)] = tile_to_place;
                            
                            hotbar_count[active_slot]--; // Remove from inventory
                            if (hotbar_count[active_slot] <= 0) {
                                hotbar_count[active_slot] = 0;
                                hotbar_id[active_slot] = 0;
                            }
                            swing_timer = 10; // Prevent hyperspeed placing
                        }
                    }
                }
            }
            
        } else {
            // Move Player
            cursor_dx = 0;
            cursor_dy = 0;
            if (KEY_DOWN(KEY_RIGHT)) { p_dx = 384; facing_left = 0; }
            if (KEY_DOWN(KEY_LEFT))  { p_dx = -384; facing_left = 1; }
            
            if (KEY_DOWN(KEY_UP) && grounded) {
                p_dy = -800; // Jump
                grounded = 0;
            }
            
            if (KEY_DOWN(KEY_B) && swing_timer == 0) {
                swing_timer = 15;
            }
        }
        
        if (swing_timer > 0) {
            swing_timer--;
            swing_frame = (14 - swing_timer) / 5; // 0, 1, 2
        }
        
        // Break foliage in front of player when swinging (Outside of Build Mode)
        if (swing_timer == 14 && !mining_mode) { 
            int fx_center = ix + (facing_left ? -8 : 16);
            int fy_center = iy + 12;
            
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int ftx = (fx_center / 8) + dx;
                    int fty = (fy_center / 8) + dy;
                    if (ftx >= 0 && ftx < WORLD_W && fty >= 0 && fty < WORLD_H) {
                        int target = world_map[fty][ftx];
                        if (target == TILE_GRASS_PLANTS) {
                            world_map[fty][ftx] = TILE_AIR;
                            hw_map[(fty % 32) * 32 + (ftx % 32)] = TILE_AIR;
                        }
                    }
                }
            }
        }
        
        // Smart Mining/Chopping Logic
        int smart_cx = -1;
        int smart_cy = -1;
        int cur_tool = hotbar_id[active_slot];

        if (!mining_mode && (cur_tool == ITEM_PICKAXE || cur_tool == ITEM_AXE)) {
            int px_tile = (ix + 12) / 8;
            int py_tile = (iy + 16) / 8;
            
            if (cur_tool == ITEM_PICKAXE) {
                if (KEY_DOWN(KEY_DOWN_BTN)) {
                    int left_tile = (ix + 10) / 8;
                    int right_tile = (ix + 21) / 8;
                    int feet_y = (iy + 30) / 8;
                    if (feet_y < WORLD_H) {
                        if (world_map[feet_y][left_tile] != TILE_AIR && world_map[feet_y][left_tile] != TILE_GRASS_PLANTS && world_map[feet_y][left_tile] != TILE_WOOD) {
                            smart_cx = left_tile; smart_cy = feet_y;
                        } else if (world_map[feet_y][right_tile] != TILE_AIR && world_map[feet_y][right_tile] != TILE_GRASS_PLANTS && world_map[feet_y][right_tile] != TILE_WOOD) {
                            smart_cx = right_tile; smart_cy = feet_y;
                        } else if (feet_y + 1 < WORLD_H && world_map[feet_y + 1][left_tile] != TILE_AIR && world_map[feet_y + 1][left_tile] != TILE_GRASS_PLANTS && world_map[feet_y + 1][left_tile] != TILE_WOOD) {
                            smart_cx = left_tile; smart_cy = feet_y + 1;
                        } else if (feet_y + 1 < WORLD_H && world_map[feet_y + 1][right_tile] != TILE_AIR && world_map[feet_y + 1][right_tile] != TILE_GRASS_PLANTS && world_map[feet_y + 1][right_tile] != TILE_WOOD) {
                            smart_cx = right_tile; smart_cy = feet_y + 1;
                        }
                    }
                } else {
                    int dir = facing_left ? -1 : 1;
                    for (int dist = 1; dist <= 3; dist++) {
                        int tx = px_tile + (dir * dist);
                        if (tx < 0 || tx >= WORLD_W) continue;
                        if (smart_cx == -1 && py_tile >= 0 && py_tile < WORLD_H && world_map[py_tile][tx] != TILE_AIR && world_map[py_tile][tx] != TILE_GRASS_PLANTS && world_map[py_tile][tx] != TILE_LEAVES && world_map[py_tile][tx] != TILE_WOOD && (world_map[py_tile][tx] < 16 || world_map[py_tile][tx] > 114)) {
                            smart_cx = tx; smart_cy = py_tile; break;
                        }
                        if (smart_cx == -1 && py_tile - 1 >= 0 && world_map[py_tile - 1][tx] != TILE_AIR && world_map[py_tile - 1][tx] != TILE_GRASS_PLANTS && world_map[py_tile - 1][tx] != TILE_LEAVES && world_map[py_tile - 1][tx] != TILE_WOOD && (world_map[py_tile - 1][tx] < 16 || world_map[py_tile - 1][tx] > 114)) {
                            smart_cx = tx; smart_cy = py_tile - 1; break;
                        }
                        if (smart_cx == -1 && py_tile + 1 < WORLD_H && world_map[py_tile + 1][tx] != TILE_AIR && world_map[py_tile + 1][tx] != TILE_GRASS_PLANTS && world_map[py_tile + 1][tx] != TILE_LEAVES && world_map[py_tile + 1][tx] != TILE_WOOD && (world_map[py_tile + 1][tx] < 16 || world_map[py_tile + 1][tx] > 114)) {
                            smart_cx = tx; smart_cy = py_tile + 1; break;
                        }
                    }
                }
            } else if (cur_tool == ITEM_AXE) {
                int dir = facing_left ? -1 : 1;
                for (int dist = 1; dist <= 3; dist++) {
                    int tx = px_tile + (dir * dist);
                    if (tx < 0 || tx >= WORLD_W) continue;
                    for (int ty_off = -2; ty_off <= 3; ty_off++) {
                        int ty = py_tile + ty_off;
                        if (ty >= 0 && ty < WORLD_H && world_map[ty][tx] == TILE_WOOD) {
                            int lowest_ty = ty;
                            while (lowest_ty + 1 < WORLD_H && world_map[lowest_ty + 1][tx] == TILE_WOOD) lowest_ty++;
                            smart_cx = tx; smart_cy = lowest_ty;
                            break;
                        }
                    }
                    if (smart_cx != -1) break;
                }
            }
            
            if (swing_timer == 14 && smart_cx != -1 && smart_cy != -1) {
                int target = world_map[smart_cy][smart_cx];
                int drop = target;
                if (drop == TILE_GRASS) drop = TILE_DIRT;
                else if (drop == TILE_COPPER) drop = ITEM_COPPER_ORE;
                else if (drop == TILE_IRON) drop = ITEM_IRON_ORE;
                else if (drop == TILE_WORKBENCH) drop = ITEM_WORKBENCH;
                else if (drop == TILE_FURNACE) drop = ITEM_FURNACE;
                else if (drop == TILE_CHEST) drop = ITEM_CHEST;
                
                int amt = 1;
                int acorns = 0;
                if (target == TILE_WOOD) {
                    if (cur_tool == ITEM_AXE) {
                        drop = TILE_PLANKS;
                        amt = 0;
                        acorns = (seed % 3);
                        int ty = smart_cy;
                        while (ty >= 0) {
                            int t_center = world_map[ty][smart_cx];
                            if (t_center == TILE_WOOD || t_center == TILE_LEAVES || (t_center >= 21 && t_center <= 119)) {
                                world_map[ty][smart_cx] = TILE_AIR;
                                hw_map[(ty % 32) * 32 + (smart_cx % 32)] = TILE_AIR;
                                for (int lx = smart_cx - 2; lx <= smart_cx + 2; lx++) {
                                    if (lx >= 0 && lx < WORLD_W) {
                                        int t = world_map[ty][lx];
                                        if (t == TILE_WOOD || t == TILE_LEAVES || (t >= 21 && t <= 119)) {
                                            world_map[ty][lx] = TILE_AIR;
                                            hw_map[(ty % 32) * 32 + (lx % 32)] = TILE_AIR;
                                        }
                                    }
                                }
                                amt++;
                                ty--;
                            } else break;
                        }
                    } else amt = 0;
                } else if (target == TILE_LEAVES || (target >= 21 && target <= 119)) {
                    amt = 0;
                } else {
                    if (cur_tool == ITEM_PICKAXE) {
                        world_map[smart_cy][smart_cx] = TILE_AIR;
                        hw_map[(smart_cy % 32) * 32 + (smart_cx % 32)] = TILE_AIR;
                    } else amt = 0;
                }
                
                for (int a = 0; a < amt + acorns; a++) {
                    int id = (a < amt) ? drop : ITEM_ACORN;
                    int added = 0;
                    for (int s = 0; s < 12; s++) {
                        if (hotbar_id[s] == id && hotbar_count[s] > 0 && hotbar_count[s] < 99) {
                            hotbar_count[s]++; added = 1; break;
                        }
                    }
                    if (!added) {
                        for (int s = 2; s < 12; s++) {
                            if (hotbar_count[s] == 0 && hotbar_id[s] != ITEM_SWORD && hotbar_id[s] != ITEM_PICKAXE && hotbar_id[s] != ITEM_AXE) {
                                hotbar_id[s] = id; hotbar_count[s] = 1; break;
                            }
                        }
                    }
                }
            }
        }
        
        if (p_dx != 0) {
            int new_x = (p_x + p_dx) >> 8;
            if (!check_collision(new_x, iy)) {
                p_x += p_dx;
            } else if (!check_collision(new_x, iy - 8)) { // Auto step-up 1 block
                p_x += p_dx;
                p_y -= 8 << 8;
            } else {
                p_dx = 0;
                p_x = (p_x & ~0xFF); // Snap to integer pixel to prevent subpixel jitter
            }
        }
        
        // Pre-check floor to prevent sub-pixel drifting into the ground
        grounded = 0;
        if (p_dy >= 0 && check_collision(ix, iy + 1)) {
            p_dy = 0;
            p_y = p_y & ~0xFF; // Clean integer snap
            grounded = 1;
        }
        
        if (p_dy != 0) {
            int new_y = (p_y + p_dy) >> 8;
            if (!check_collision(ix, new_y)) {
                p_y += p_dy;
            } else {
                if (p_dy > 0) {
                    grounded = 1; // Hit floor from fall
                }
                p_dy = 0;
                p_y = (p_y & ~0xFF); // Snap
            }
        }
        
        // Final integer coords
        ix = p_x >> 8;
        iy = p_y >> 8;
        
        // Smooth camera follow
        int target_cam_x = ix - 120 + 16;
        int target_cam_y = iy - 80 + 16;
        cam_x += (target_cam_x - cam_x) / 8;
        cam_y += (target_cam_y - cam_y) / 8;

        // Camera bounds
        if (cam_x < 0) cam_x = 0;
        if (cam_y < 0) cam_y = 0;
        if (cam_x > (WORLD_W * 8) - 240) cam_x = (WORLD_W * 8) - 240;
        if (cam_y > (WORLD_H * 8) - 160) cam_y = (WORLD_H * 8) - 160;

        // Animate (Just bob the sprite up and down instead of switching broken frames)
        if (!grounded) {
             current_frame = 1; // Jump pose uses a 1px bob offset
        } else if (p_dx != 0 && !mining_mode) {
             frame_timer++;
             if (frame_timer > 6) {
                 frame_timer = 0;
                 current_frame = !current_frame; // Bob up and down to simulate running
             }
        } else {
             current_frame = 0; // Idle
        }

        // 5. Update Slimes
        if (!inv_open) {
            // Reduced spawn frequency (1 in 2000 frames)
            if ((seed % 2000) < 1) { 
                for (int i = 0; i < MAX_SLIMES; i++) {
                    if (!slimes[i].active) {
                        int rx_off = (seed % 240) - 120;
                        if (rx_off > -30 && rx_off < 30) rx_off = 60; // Safety zone
                        int rx = (p_x >> 8) + rx_off;
                        if (rx < 0) rx = 0; if (rx >= WORLD_W*8) rx = WORLD_W*8-1;
                        
                        // Find a floor near player's depth
                        int tx = rx / 8;
                        int py_tile = (p_y >> 11); // Start search slightly above player
                        int start_ty = py_tile - 10;
                        if (start_ty < 1) start_ty = 1;
                        
                        int found_y = -1;
                        for (int ty = start_ty; ty < WORLD_H; ty++) {
                            // Find first transition from AIR to SOLID
                            if (world_map[ty][tx] != TILE_AIR && world_map[ty-1][tx] == TILE_AIR) {
                                found_y = (ty * 8) - 15; 
                                break;
                            }
                        }

                        if (found_y != -1) {
                            slimes[i].x = rx << 8;
                            slimes[i].y = found_y << 8;
                            slimes[i].dx = 0; slimes[i].dy = 0;
                            slimes[i].type = (found_y / 8 > 35) ? 1 : 0;
                            slimes[i].hp = (slimes[i].type == 1) ? 35 : 14; 
                            slimes[i].active = 1;
                            slimes[i].jump_timer = 60 + (seed % 60);
                            slimes[i].flicker = 0;
                            slimes[i].stuck_timer = 0;
                            slimes[i].stuck_x = slimes[i].x >> 8;
                            slimes[i].stuck_y = slimes[i].y >> 8;
                            break;
                        }
                    }
                }
            }

            for (int i = 0; i < MAX_SLIMES; i++) {
                if (!slimes[i].active) continue;
                
                int p_sx = p_x >> 8;
                int p_sy = p_y >> 8;
                int sx = slimes[i].x >> 8;
                int sy = slimes[i].y >> 8;
                
                // Stricter despawning
                int dist_x = sx - p_sx;
                int dist_y = sy - p_sy;
                if (abs(dist_x) > 320 || abs(dist_y) > 200) {
                    slimes[i].active = 0;
                    continue;
                }
                
                slimes[i].dy += 24;
                if (slimes[i].dy > 1024) slimes[i].dy = 1024;
                
                // Check stuck logic
                if (slimes[i].stuck_timer > 300) {
                    // Ignore collision for this jump to get unstuck
                    slimes[i].y += slimes[i].dy;
                    sy = slimes[i].y >> 8;
                    slimes[i].x += slimes[i].dx;
                    
                    // Reset unstuck only when it lands safely
                    if (is_solid(sx + 16, sy + 32) && slimes[i].dy > 0) {
                        slimes[i].stuck_timer = 0;
                    }
                } else {
                    // Normal Update Y
                    if (slimes[i].dy > 0 && is_solid(sx + 8, sy + 16)) {
                        slimes[i].dy = 0;
                        slimes[i].dx = 0;
                        slimes[i].y &= ~0xFF; // snap
                    } else {
                        int next_sy = (slimes[i].y + slimes[i].dy) >> 8;
                        if (slimes[i].dy < 0 && (is_solid(sx + 4, next_sy + 3) || is_solid(sx + 12, next_sy + 3))) {
                            slimes[i].dy = 0; // Bonk head on ceiling
                        } else {
                            slimes[i].y += slimes[i].dy;
                        }
                    }
                    
                    sy = slimes[i].y >> 8; // Update sy for X collision
                    
                    // Normal Update X with wall bouncing
                    if (slimes[i].dx != 0) {
                        int next_sx = (slimes[i].x + slimes[i].dx) >> 8;
                        int side_x = (slimes[i].dx > 0) ? (next_sx + 15) : (next_sx);
                        
                        // Check mid and bottom parts of slime body against walls
                        if (is_solid(side_x, sy + 8) || is_solid(side_x, sy + 15)) {
                            slimes[i].dx = -slimes[i].dx; // Bounce!
                        } else {
                            slimes[i].x += slimes[i].dx;
                        }
                    }
                }
                
                // If on ground (check again after move)
                sx = slimes[i].x >> 8;
                sy = slimes[i].y >> 8;
                if (is_solid(sx + 8, sy + 16)) {
                    // Update stuck detection
                    int dx_stuck = sx - slimes[i].stuck_x;
                    int dy_stuck = sy - slimes[i].stuck_y;
                    if (dx_stuck*dx_stuck + dy_stuck*dy_stuck < 16*16) {
                        slimes[i].stuck_timer++;
                    } else {
                        slimes[i].stuck_timer = 0;
                        slimes[i].stuck_x = sx;
                        slimes[i].stuck_y = sy;
                    }
                    
                    if (slimes[i].jump_timer > 0) slimes[i].jump_timer--;
                    else {
                        slimes[i].dy = -500 - (seed % 150);
                        slimes[i].dx = (p_sx > sx) ? 256 : -256;
                        // If stuck, jump straight UP to try clearing blocks
                        if (slimes[i].stuck_timer > 300) {
                            slimes[i].dy = -800; // Big jump up
                        }
                        slimes[i].jump_timer = 90 + (seed % 60); // Jump much less often
                    }
                }
                
                if (slimes[i].flicker > 0) slimes[i].flicker--;

                // Player damage from slime
                int s_left = sx;
                int s_right = sx + 16;
                int s_top = sy + 3;
                int s_bottom = sy + 16;

                if (p_flicker == 0 && !inv_open) {
                    int p_left = p_sx + 10;
                    int p_right = p_sx + 21;
                    int p_top = p_sy + 7;
                    int p_bottom = p_sy + 29;

                    if (p_right > s_left && p_left < s_right && p_bottom > s_top && p_top < s_bottom) {
                        if (p_health > 0) p_health--;
                        p_flicker = 60; // 1 second invincibility
                        p_dy = -400; // Knockback
                        p_dx = (p_sx > sx) ? 400 : -400;
                    }
                }

                if (swing_timer > 0 && hotbar_id[active_slot] == ITEM_SWORD && slimes[i].flicker == 0) {
                    int sw_left = facing_left ? (p_sx) : (p_sx + 16);
                    int sw_right = facing_left ? (p_sx + 16) : (p_sx + 32);
                    int sw_top = p_sy + 8;
                    int sw_bottom = p_sy + 32;

                    if (sw_right > s_left && sw_left < s_right && sw_bottom > s_top && sw_top < s_bottom) {
                        slimes[i].hp -= (10 + (seed % 3)); // 10-12 damage
                        slimes[i].flicker = 10;
                        slimes[i].dy = -300;
                        slimes[i].dx = (p_sx > sx) ? -512 : 512;
                        if (slimes[i].hp <= 0) {
                            slimes[i].active = 0;
                            int gel_amt = seed % 4; // 0-3
                            for(int g=0; g<gel_amt; g++) {
                                for(int s=0; s<12; s++) {
                                    if(hotbar_id[s] == ITEM_GEL) { if(hotbar_count[s] < 99) { hotbar_count[s]++; break; } }
                                    else if(hotbar_count[s] == 0 && hotbar_id[s] != ITEM_SWORD && hotbar_id[s] != ITEM_PICKAXE) {
                                        hotbar_id[s] = ITEM_GEL; hotbar_count[s] = 1; break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if (p_flicker > 0) p_flicker--;

        // Draw Player Sprite
        // 8bpp (0x2000), 32x32 size (Attr1: 0x8000), shape Square (Attr0: 0x0000)
        // Horizontal Flip (Attr1: 0x1000)
        int draw_x = ix - cam_x;
        int draw_y = iy - cam_y - current_frame; // Bob up slightly
        
        if (draw_x > -32 && draw_x < 240 && draw_y > -32 && draw_y < 160 && (p_flicker % 4 < 2)) {
            oam[0].attr0 = (draw_y & 0x00FF) | 0x2000; // 256 colors
            oam[0].attr1 = (draw_x & 0x01FF) | 0x8000 | (facing_left ? 0x1000 : 0);
            oam[0].attr2 = 0; // We only have 1 frame packed now, tile index 0
        } else {
            oam[0].attr0 = 0x0200; // Hide if offscreen
        }
        
        // Draw Mining Cursor
        if (mining_mode) {
            // Cursor tile index: Player (16), Heart (4), Cursor is at index 20 tiles => attr2 = 40
            int cx = (((ix + 16) / 8) * 8) + (cursor_dx * 8) - cam_x;
            int cy = (((iy + 16) / 8) * 8) + (cursor_dy * 8) - cam_y;
            oam[1].attr0 = (cy & 0x00FF) | 0x2000;
            oam[1].attr1 = (cx & 0x01FF) | 0x0000; // 8x8 size
            oam[1].attr2 = 40; 
        } else {
            oam[1].attr0 = 0x0200;
        }

        // Draw Health Bar (5 Hearts)
        // Heart starts at tile 16 => attr2 = 32
        for (int i = 0; i < 5; i++) {
            if (i < p_health && !inv_open) {
                int h_x = 240 - 20 - (i * 14); // right to left
                int h_y = 6;
                oam[i + 2].attr0 = (h_y & 0x00FF) | 0x2000;
                oam[i + 2].attr1 = (h_x & 0x01FF) | 0x4000; // 16x16 size
                oam[i + 2].attr2 = 32;
            } else {
                oam[i + 2].attr0 = 0x0200; // Disable
            }
        }
        
        // Draw Inventory / Hotbar Grid
        if (inv_mode == 0) { 
            int num_slots = inv_open ? 12 : 4;
            for (int i = 0; i < num_slots; i++) {
                int slot_x = i % 4;
                int slot_y = i / 4;
                int bx = 4 + (slot_x * 28);
                int by = 4 + (slot_y * 28);
                
                // 1. Selector highlight
                int is_selected = (inv_open ? (i == inv_cursor) : (i == active_slot));
                int is_held = (inv_open && i == inv_held_cursor);
                if (is_selected || is_held) {
                    oam[i + 44].attr0 = (by & 0x00FF) | 0x2000;
                    oam[i + 44].attr1 = (bx & 0x01FF) | 0x8000;
                    oam[i + 44].attr2 = is_held ? 74 : 74; // We can use the same visual for now, or just pulses
                    // If both, let's flicker or just show selected
                    if (is_selected && is_held) oam[i+44].attr0 |= 0x0200; // Flicker or hide selected to show held
                    else oam[i+44].attr0 &= ~0x0200;
                } else {
                    oam[i + 44].attr0 = 0x0200;
                }
                
                // 2. Draw Item
                int item_base = 0;
                int id = hotbar_id[i];
                if (id == ITEM_SWORD) item_base = 106;
                else if (id == ITEM_PICKAXE) item_base = 122;
                else if (id == ITEM_AXE) item_base = 138;
                else if (id == ITEM_TORCH || id == TILE_TORCH) item_base = 154;
                else if (hotbar_count[i] > 0) {
                    if (id == TILE_DIRT) item_base = 170; 
                    else if (id == TILE_STONE) item_base = 178; 
                    else if (id == TILE_WOOD) item_base = 186; 
                    else if (id == TILE_PLANKS) item_base = 194; 
                    else if (id == TILE_MUD) item_base = 202;
                    else if (id == TILE_JUNGLE_GRASS) item_base = 210;
                    else if (id == ITEM_ACORN) item_base = 218;
                    else if (id == ITEM_GEL) item_base = 258;
                    else if (id == ITEM_COPPER_ORE) item_base = 266;
                    else if (id == ITEM_IRON_ORE) item_base = 274;
                    else if (id == ITEM_COPPER_BAR) item_base = 282;
                    else if (id == ITEM_IRON_BAR) item_base = 290;
                    else if (id == ITEM_WORKBENCH) item_base = 298;
                    else if (id == ITEM_FURNACE) item_base = 306;
                    else if (id == ITEM_CHEST) item_base = 314;
                }
                
                if (item_base > 0) {
                    int item_x = bx + 8; int item_y = by + 8;
                    oam[i + 32].attr0 = (item_y & 0x00FF) | 0x2000;
                    oam[i + 32].attr1 = (item_x & 0x01FF) | 0x4000; 
                    oam[i + 32].attr2 = item_base; 
                }
 else {
                    oam[i + 32].attr0 = 0x0200;
                }

                // 3. Draw Background (Lowest)
                oam[i + 56].attr0 = (by & 0x00FF) | 0x2000;
                oam[i + 56].attr1 = (bx & 0x01FF) | 0x8000; 
                oam[i + 56].attr2 = 42; 
                
                // 4. Draw Stack sizes (Highest priority, draws on top)
                if (id != ITEM_SWORD && id != ITEM_PICKAXE && id != ITEM_AXE && hotbar_count[i] > 0) {
                    int tens = hotbar_count[i] / 10;
                    int ones = hotbar_count[i] % 10;
                    int num_y = by + 22;
                    int num_x = bx + 19;
                    
                    if (tens > 0) {
                        oam[i*2 + 8].attr0 = (num_y & 0x00FF) | 0x2000;
                        oam[i*2 + 8].attr1 = ((num_x - 4) & 0x01FF) | 0x0000;
                        oam[i*2 + 8].attr2 = 322 + (tens * 2);
                    } else { oam[i*2 + 8].attr0 = 0x0200; }
                    
                    oam[i*2 + 9].attr0 = (num_y & 0x00FF) | 0x2000;
                    oam[i*2 + 9].attr1 = (num_x & 0x01FF) | 0x0000;
                    oam[i*2 + 9].attr2 = 322 + (ones * 2);
                } else {
                    oam[i*2 + 8].attr0 = 0x0200; oam[i*2 + 9].attr0 = 0x0200;
                }
            }
        } else if (inv_open && inv_mode == 1) {
            // Crafting Interface
            for(int r=0; r<2; r++) {
                int rx = 4;
                int ry = 4 + (r * 32);
                
                // Background
                oam[r + 56].attr0 = (ry & 0x00FF) | 0x2000;
                oam[r + 56].attr1 = (rx & 0x01FF) | 0x8000;
                oam[r + 56].attr2 = 42;
                
                // Result Item
                int res_id = (r == 0) ? 0 : ITEM_TORCH; // Slot 0 removed
                int res_base = (r == 0) ? 0 : 330;
                oam[r + 32].attr0 = (ry & 0x00FF) | 0x2000;
                oam[r + 32].attr1 = (rx & 0x01FF) | 0x8000;
                oam[r + 32].attr2 = res_base;
                
                // Show current count of result item in inventory
                int current_total = 0;
                for(int s=0; s<12; s++) if(hotbar_id[s] == res_id) current_total += hotbar_count[s];
                if(current_total > 99) current_total = 99;

                if (current_total > 0) {
                    int tens = current_total / 10;
                    int ones = current_total % 10;
                    int num_y = ry + 22; int num_x = rx + 19;
                    if (tens > 0) {
                        oam[r*2 + 8].attr0 = (num_y & 0x00FF) | 0x2000;
                        oam[r*2 + 8].attr1 = ((num_x - 4) & 0x01FF) | 0x0000;
                    oam[r*2 + 8].attr2 = 322 + (tens * 2);
                } else { oam[r*2 + 8].attr0 = 0x0200; }
                oam[r*2 + 9].attr0 = (num_y & 0x00FF) | 0x2000;
                oam[r*2 + 9].attr1 = (num_x & 0x01FF) | 0x0000;
                oam[r*2 + 9].attr2 = 322 + (ones * 2);
                } else {
                    oam[r*2 + 8].attr0 = 0x0200; oam[r*2 + 9].attr0 = 0x0200;
                }

                if (inv_cursor == r) {
                    oam[r + 44].attr0 = (ry & 0x00FF) | 0x2000;
                    oam[r + 44].attr1 = (rx & 0x01FF) | 0x8000;
                    oam[r + 44].attr2 = 74;
                } else { oam[r + 44].attr0 = 0x0200; }
            }
            // Hide other UI
            for (int i = 2; i < 12; i++) {
                oam[i + 32].attr0 = 0x0200; oam[i + 44].attr0 = 0x0200; oam[i + 56].attr0 = 0x0200;
                oam[i*2 + 8].attr0 = 0x0200; oam[i*2 + 9].attr0 = 0x0200;
            }
        }
        
        // Hide unused slots when closed
        if (!inv_open) {
            for (int i = 0; i < 12; i++) {
                // Keep hotbar icons visible if not open? No, the loop above handles hotbar.
                // We just need to make sure 4-11 are hidden.
                if (i >= 4) {
                    oam[i + 32].attr0 = 0x0200;
                    oam[i + 44].attr0 = 0x0200;
                    oam[i + 56].attr0 = 0x0200;
                    oam[i*2 + 8].attr0 = 0x0200;
                    oam[i*2 + 9].attr0 = 0x0200;
                }
            }
            inv_mode = 0; 
            inv_held_cursor = -1; // Reset held state
        }
        
        if (swing_timer > 0) {
            int cur_id = hotbar_id[active_slot];
            int item_base = 0;
            if (cur_id == ITEM_SWORD) item_base = 106;
            else if (cur_id == ITEM_PICKAXE) item_base = 122;
            else if (cur_id == ITEM_AXE) item_base = 138;
            else if (cur_id == ITEM_TORCH) item_base = 154;
            
            if (item_base > 0) {
                if (facing_left) item_base += 8; // Use Flipped frame (4 tiles * 2 indices)
                
                // Show in front of character (offset based on direction)
                int wx = draw_x + (facing_left ? -4 : 20);
                int wy = draw_y + 8;
                oam[30].attr0 = (wy & 0x00FF) | 0x2000;
                oam[30].attr1 = (wx & 0x01FF) | 0x4000; 
                oam[30].attr2 = item_base; 
            } else {
                oam[30].attr0 = 0x0200;
            }
        } else {
            oam[30].attr0 = 0x0200;
        }

        // Set hardware scroll registers
        REG_BG0HOFS = cam_x;
        REG_BG0VOFS = cam_y;
        
        static int last_bg_type = -1;
        int current_bg_type = 0; // 0=Sky, 1=BG2, 2=BG3
        if (cam_y > 280 && cam_y <= 600) current_bg_type = 1;
        else if (cam_y > 600) current_bg_type = 2;
        
        if (current_bg_type != last_bg_type) {
            if (current_bg_type == 0) {
                for (int i = 0; i < 32 * 32; i++) bg_map[i] = 0;
                draw_clouds();
            } else if (current_bg_type == 1) {
                fill_bg_map(120); 
            } else if (current_bg_type == 2) {
                fill_bg_map(156); 
            }
            last_bg_type = current_bg_type;
        }
        
        // Background Parallax (slower scroll)
        REG_BG1HOFS = cam_x / 2;
        REG_BG1VOFS = cam_y / 2;

        // Draw Slimes
        for(int i=0; i<MAX_SLIMES; i++) {
            if (slimes[i].active && (slimes[i].flicker % 2 == 0)) {
                int sx = (slimes[i].x >> 8) - cam_x;
                int sy = (slimes[i].y >> 8) - cam_y;
                if (sx > -32 && sx < 240 && sy > -32 && sy < 160) {
                    oam[i + 80].attr0 = (sy & 0x00FF) | 0x2000;
                    oam[i + 80].attr1 = (sx & 0x01FF) | 0x4000; // 16x16 size
                    int base_tile = (slimes[i].type == 1) ? 242 : 226;
                    oam[i + 80].attr2 = (slimes[i].dy < 0) ? (base_tile + 8) : base_tile; 
                } else oam[i + 80].attr0 = 0x0200;
            } else oam[i + 80].attr0 = 0x0200;
        }

        // Draw Sun (Loosely follow player)
        if (current_bg_type == 0) {
            int sun_sx = 180 - (cam_x / 16);
            int sun_sy = -10 - (cam_y / 16);
            oam[127].attr0 = (sun_sy & 0x00FF) | 0x2000;
            oam[127].attr1 = (sun_sx & 0x01FF) | 0x8000;
            oam[127].attr2 = 342 | 0x0800; // Priority 2 (behind clouds)
        } else {
            oam[127].attr0 = 0x0200; // Hide
        }
        
        // Draw Smart Cursor
        if (smart_cx != -1 && smart_cy != -1) {
            int cx_draw = (smart_cx * 8) - cam_x;
            int cy_draw = (smart_cy * 8) - cam_y;
            if (cx_draw > -8 && cx_draw < 240 && cy_draw > -8 && cy_draw < 160) {
                oam[126].attr0 = (cy_draw & 0x00FF) | 0x2000;
                oam[126].attr1 = (cx_draw & 0x01FF) | 0x0000; // 8x8 size
                oam[126].attr2 = 374; 
            } else {
                oam[126].attr0 = 0x0200;
            }
        } else {
            oam[126].attr0 = 0x0200;
        }

        // Update tilemap
        update_map_seam(prev_x, prev_y, cam_x, cam_y);
        prev_x = cam_x;
        prev_y = cam_y;

        // Tree Growth Tick (Target: 1/1500 per sapling per frame)
        for (int k = 0; k < 22; k++) {
            int rx = rand() % WORLD_W;
            int ry = rand() % (WORLD_H - 1);
            if (world_map[ry][rx] == TILE_SAPLING) {
                grow_tree(rx, ry + 1);
            }
        }
    }

    return 0;
}
