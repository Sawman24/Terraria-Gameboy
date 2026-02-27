#include <stdint.h>
#include <stdlib.h>
#include "world_gen.h"
#include "tileset.h"
#include "sprite_gfx.h"
#include "audio.h"
#include "theme_data.h"
#include "sfx_data.h"

#define ITEM_SWORD    100
#define ITEM_PICKAXE  101

#define TILE_DOOR_T 29
#define TILE_DOOR_M 30
#define TILE_DOOR_B 31

#define MAX_SLIMES 8
typedef struct {
    int x, y, dx, dy;
    int hp;
    int active;
    int type; //  green=0, blue=1
    int jump_timer;
    int flicker;
    int stuck_timer;
    int stuck_x;
    int stuck_y;
} Slime;

#define MAX_CHESTS 32
typedef struct {
    int x, y; // Tile coordinates
    int items_id[12];
    int items_count[12];
    int active;
} Chest;

Slime slimes[MAX_SLIMES];
Chest chests[MAX_CHESTS];
int open_chest_id = -1; // -1 if no chest is open
int inv_cursor = 0; // shared
// Unused globals removed: inv_held_item_id, inv_held_item_count


typedef struct {
    int res_id;
    int res_qty;
    int sprite_base;
    int ing_id[3];
    int ing_qty[3];
    int station_tile;
} Recipe;

const Recipe recipes[] = {
    {ITEM_TORCH, 3, 170, {ITEM_GEL, ITEM_PLANKS, 0}, {1, 1, 0}, 0},
    {ITEM_WORKBENCH, 1, 322, {ITEM_PLANKS, 0, 0}, {10, 0, 0}, 0},
    {ITEM_FURNACE, 1, 330, {ITEM_STONE, ITEM_PLANKS, ITEM_TORCH}, {20, 4, 3}, TILE_WORKBENCH},
    {ITEM_COPPER_BAR, 1, 306, {ITEM_COPPER_ORE, 0, 0}, {3, 0, 0}, TILE_FURNACE},
    {ITEM_IRON_BAR, 1, 314, {ITEM_IRON_ORE, 0, 0}, {3, 0, 0}, TILE_FURNACE},
    {ITEM_CHEST, 1, 338, {ITEM_PLANKS, ITEM_IRON_BAR, 0}, {8, 2, 0}, TILE_WORKBENCH},
    {ITEM_GOLD_BAR, 1, 394, {ITEM_GOLD_ORE, 0, 0}, {3, 0, 0}, TILE_FURNACE},
    {ITEM_DOOR, 1, 402, {ITEM_PLANKS, 0, 0}, {6, 0, 0}, TILE_WORKBENCH},
    {ITEM_CHAIR, 1, 418, {ITEM_PLANKS, 0, 0}, {4, 0, 0}, TILE_WORKBENCH},
    {ITEM_ANVIL, 1, 410, {ITEM_IRON_BAR, 0, 0}, {5, 0, 0}, TILE_WORKBENCH},
    {ITEM_TABLE, 1, 426, {ITEM_PLANKS, 0, 0}, {8, 0, 0}, TILE_WORKBENCH}
};
#define NUM_RECIPES 11

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

// Background/Window Control
#define BG_256_COLOR    0x0080
#define BG_SIZE_32x32   0x0000
#define BG_TILE_BASE(n) ((n) << 2)
#define BG_MAP_BASE(n)  ((n) << 8)
#define REG_WIN0H       (*(volatile uint16_t*)0x04000040)
#define REG_WIN0V       (*(volatile uint16_t*)0x04000044)
#define REG_WIN1H       (*(volatile uint16_t*)0x04000042)
#define REG_WIN1V       (*(volatile uint16_t*)0x04000046)
#define REG_WININ       (*(volatile uint16_t*)0x04000048)
#define REG_WINOUT      (*(volatile uint16_t*)0x0400004A)
#define WIN0_ENABLE     0x2000
#define WIN1_ENABLE     0x4000
#define WINOBJ_ENABLE   0x8000
#define WIN_EFFECT      0x0020

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

int hotbar_id[12] = {ITEM_SWORD, ITEM_PICKAXE, ITEM_AXE, ITEM_DIRT, 0, 0, 0, 0, 0, 0, 0, 0};
int hotbar_count[12] = {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int p_x = 0;
int p_y = 0;
int spawn_x = 0;
int spawn_y = 0;
int p_health = 100;

int inv_open = 0;
static unsigned int game_time = 0;
static int current_darkness = 0;
unsigned char tile_solid_lut[512];

static unsigned short blend_color(unsigned short c1, unsigned short c2, int ratio) {
    int r1 = c1 & 31, g1 = (c1 >> 5) & 31, b1 = (c1 >> 10) & 31;
    int r2 = c2 & 31, g2 = (c2 >> 5) & 31, b2 = (c2 >> 10) & 31;
    int r = r1 + ((r2 - r1) * ratio >> 8);
    int g = g1 + ((g2 - g1) * ratio >> 8);
    int b = b1 + ((b2 - b1) * ratio >> 8);
    return (unsigned short)(r | (g << 5) | (b << 10));
}

void update_day_night() {
    game_time = (game_time + 1) % 72000; // 20 min @ 60fps
    
    unsigned short sky;
    int darkness = 0;
    
    // Colors (GBA 15-bit BGR)
    unsigned short day_sky     = 0x7EB0; // Light Blue
    unsigned short sunset_sky  = 0x15FF; // Orange
    unsigned short night_sky   = 0x1442; // Dark Blue/Black
    unsigned short sunrise_sky = 0x3D5F; // Pink/Red
    
    if (game_time < 32000) { // Full Day
        sky = day_sky;
        darkness = 0;
    } else if (game_time < 36000) { // Sunset Phase 1 (Day -> Orange)
        int ratio = ((game_time - 32000) * 256) / 4000;
        sky = blend_color(day_sky, sunset_sky, ratio);
        darkness = (ratio * 6) >> 8; 
    } else if (game_time < 40000) { // Sunset Phase 2 (Orange -> Night)
        int ratio = ((game_time - 36000) * 256) / 4000;
        sky = blend_color(sunset_sky, night_sky, ratio);
        darkness = 6 + ((ratio * 8) >> 8); 
    } else if (game_time < 68000) { // Full Night
        sky = night_sky;
        darkness = 14; 
    } else if (game_time < 72000) { // Sunrise (Night -> Pink -> Day)
        int phase = game_time - 68000;
        if (phase < 2000) { // Night -> Pink
            int ratio = (phase * 256) / 2000;
            sky = blend_color(night_sky, sunrise_sky, ratio);
            darkness = 14 - ((ratio * 7) >> 8); 
        } else { // Pink -> Day
            int ratio = ((phase - 2000) * 256) / 2000;
            sky = blend_color(sunrise_sky, day_sky, ratio);
            darkness = 7 - ((ratio * 7) >> 8); 
        }
    } else {
        sky = day_sky;
        darkness = 0;
    }
    
    MEM_PAL_BG[0] = sky;
    REG_BLDCNT = 0x00FF; // Darken effect (Targets all)
    REG_BLDY = (unsigned short)darkness;
    current_darkness = darkness;
    
    if (darkness > 0) {
        REG_WININ = 0x1F1F;  // Inside Win0/1: Layers ON, Effect OFF
        
        // WINOUT: Low Byte = Outside all windows, High Byte = Inside OBJWINDOW
        // We want Effects ON outside (0x3F) and OFF inside OBJWin (0x1F)
        REG_WINOUT = 0x1F3F; 

        if (inv_open) {
            // Protect full inventory/chest grid plus crafting
            REG_WIN0H = (4 << 8) | 236; 
            REG_WIN0V = (4 << 8) | 92; // Top=4, Bottom=92
            REG_WIN1H = 0; REG_WIN1V = 0;
        } else {
            // Surgical: Hotbar Box (Left)
            REG_WIN0H = (4 << 8) | 116; 
            REG_WIN0V = (4 << 8) | 32; // Top=4, Bottom=32
            // Surgical: Hearts Bar (Right)
            REG_WIN1H = (168 << 8) | 238; 
            REG_WIN1V = (4 << 8) | 22; // Top=4, Bottom=22
        }
    } else {
        REG_WININ = 0x3F3F;
        REG_WINOUT = 0x3F3F;
        REG_WIN0H = 0; REG_WIN0V = 0;
        REG_WIN1H = 0; REG_WIN1V = 0;
    }
}

int is_near_station(int target_tile) {
    if (target_tile == 0) return 1;
    int tx = (p_x >> 8) / 8;
    int ty = (p_y >> 8) / 8;
    for (int y = ty - 5; y <= ty + 5; y++) {
        for (int x = tx - 5; x <= tx + 5; x++) {
            if (x >= 0 && x < WORLD_W && y >= 0 && y < WORLD_H) {
                int t = world_map[y][x];
                if (t == target_tile) return 1;
                if (target_tile == TILE_WORKBENCH && t == TILE_WORKBENCH_R) return 1;
                if (target_tile == TILE_FURNACE && (t == TILE_FURNACE_TM || t == TILE_FURNACE_TR || t == TILE_FURNACE_BL || t == TILE_FURNACE_BM || t == TILE_FURNACE_BR)) return 1;
                if (target_tile == TILE_CHEST && (t == TILE_CHEST_TR || t == TILE_CHEST_BL || t == TILE_CHEST_BR)) return 1;
            }
        }
    }
    return 0;
}

void add_item(int id, int qty) {
    if (id <= 0) return;
    for (int q = 0; q < qty; q++) {
        int added = 0;
        for (int s = 0; s < 12; s++) {
            if (hotbar_id[s] == id && hotbar_count[s] > 0 && hotbar_count[s] < 99) {
                hotbar_count[s]++; added = 1; break;
            }
        }
        if (!added) {
            for (int s = 0; s < 12; s++) {
                if (hotbar_count[s] == 0 && hotbar_id[s] != ITEM_SWORD && hotbar_id[s] != ITEM_PICKAXE && hotbar_id[s] != ITEM_AXE) {
                    hotbar_id[s] = id; hotbar_count[s] = 1; added = 1; break;
                }
            }
        }
    }
}


// Cloud indices in the current tileset
#define BG_DIRT_WALL_TILE  122
#define BG_STONE_WALL_TILE 158
#define CLOUD0_TILE 194
#define CLOUD1_TILE 226
#define CLOUD2_TILE 238
#define FONT_BASE_TILE 217
#define SUN_TILE       227
#define SMART_CURSOR_TILE 243
#define MASK_BASE_TILE 244

static void draw_clouds() {
    // Cloud 0: 8x4 tiles
    for(int ty=0; ty<4; ty++) for(int tx=0; tx<8; tx++) bg_map[(2 + ty) * 32 + (2 + tx)] = CLOUD0_TILE + (ty * 8) + tx;
    // Cloud 1: 6x2 tiles
    for(int ty=0; ty<2; ty++) for(int tx=0; tx<6; tx++) bg_map[(6 + ty) * 32 + (15 + tx)] = CLOUD1_TILE + (ty * 6) + tx;
    // Cloud 2: 5x3 tiles
    for(int ty=0; ty<3; ty++) for(int tx=0; tx<5; tx++) bg_map[(3 + ty) * 32 + (24 + tx)] = CLOUD2_TILE + (ty * 5) + tx;
}


int find_chest(int x, int y) {
    for (int i = 0; i < MAX_CHESTS; i++) {
        if (!chests[i].active) continue;
        if (x >= chests[i].x && x <= chests[i].x + 1 && y >= chests[i].y && y <= chests[i].y + 1) return i;
    }
    return -1;
}

int add_chest(int x, int y) {
    int idx = -1;
    for (int i = 0; i < MAX_CHESTS; i++) {
        if (!chests[i].active) { idx = i; break; }
    }
    if (idx != -1) {
        chests[idx].x = x; chests[idx].y = y; chests[idx].active = 1;
        for (int j = 0; j < 12; j++) { chests[idx].items_id[j] = 0; chests[idx].items_count[j] = 0; }
    }
    return idx;
}

void remove_chest(int x, int y) {
    int idx = find_chest(x, y);
    if (idx != -1) {
        for (int j = 0; j < 12; j++) {
            if (chests[idx].items_id[j] > 0) add_item(chests[idx].items_id[j], chests[idx].items_count[j]);
        }
        chests[idx].active = 0;
    }
}

int get_comb_id(int idx) {
    if (idx < 12) return hotbar_id[idx];
    if (open_chest_id != -1 && idx < 24) return chests[open_chest_id].items_id[idx-12];
    return 0;
}
int get_comb_count(int idx) {
    if (idx < 12) return hotbar_count[idx];
    if (open_chest_id != -1 && idx < 24) return chests[open_chest_id].items_count[idx-12];
    return 0;
}
void set_comb_id(int idx, int id) {
    if (idx < 12) hotbar_id[idx] = id;
    else if (open_chest_id != -1 && idx < 24) chests[open_chest_id].items_id[idx-12] = id;
}
void set_comb_count(int idx, int count) {
    if (idx < 12) hotbar_count[idx] = count;
    else if (open_chest_id != -1 && idx < 24) chests[open_chest_id].items_count[idx-12] = count;
}


int available_recipes[NUM_RECIPES];
int num_available = 0;

void update_available_recipes(int* hotbar_id, int* hotbar_count) {
    num_available = 0;
    for (int i = 0; i < NUM_RECIPES; i++) {
        int can = 1;
        for (int j = 0; j < 3; j++) {
            int rid = recipes[i].ing_id[j];
            if (rid > 0) {
                int count = 0;
                for(int s=0; s<12; s++) if(hotbar_id[s] == rid) count += hotbar_count[s];
                if (count < recipes[i].ing_qty[j]) { can = 0; break; }
            }
        }
        if (can && is_near_station(recipes[i].station_tile)) available_recipes[num_available++] = i;
    }
}

static void fill_bg_map(int start_tile) {
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            int tx = x % 6;
            int ty = y % 6;
            bg_map[y * 32 + x] = start_tile + (ty * 6) + tx;
        }
    }
}

// Function draw_clouds removed (unused)

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

// DMA bit flags
#define DMA_DEST_FIXED  0x02000000
#define DMA_REPEAT      0x02000000
#define DMA_SOURCE_FIXED 0x01000000
#define DMA_IRQ         0x40000000

// Quick DMA3 Copy
static inline void dma_copy(const void* src, void* dest, uint16_t count, uint32_t mode) {
    REG_DMA3SAD = (uint32_t)src;
    REG_DMA3DAD = (uint32_t)dest;
    REG_DMA3CNT = count | mode | DMA_ENABLE;
}

void update_map_seam(int prev_cx, int prev_cy, int cx, int cy) {
    int cur_tx = cx >> 3;
    int cur_ty = cy >> 3;
    int old_tx = prev_cx >> 3;
    int old_ty = prev_cy >> 3;

    // Full redraw if first run or large jump
    if (prev_cx == -999 || abs(cur_tx - old_tx) > 10 || abs(cur_ty - old_ty) > 10) {
        for (int y = -2; y < 22; y++) {
            for (int x = -1; x < 32; x++) {
                int map_y = cur_ty + y;
                int map_x = cur_tx + x;
                if (map_x >= 0 && map_x < WORLD_W && map_y >= 0 && map_y < WORLD_H) {
                    hw_map[(map_y & 31) * 32 + (map_x & 31)] = world_map[map_y][map_x];
                }
            }
        }
        return;
    }

    if (cur_tx == old_tx && cur_ty == old_ty) return;

    // Horizontal Seam
    if (cur_tx > old_tx) { // Moved Right
        int x = cur_tx + 30;
        int hw_x = x & 31;
        for (int y = cur_ty - 2; y < cur_ty + 22; y++) {
            if (x >= 0 && x < WORLD_W && y >= 0 && y < WORLD_H)
                hw_map[(y & 31) * 32 + hw_x] = world_map[y][x];
        }
    } else if (cur_tx < old_tx) { // Moved Left
        int x = cur_tx - 1;
        int hw_x = x & 31;
        for (int y = cur_ty - 2; y < cur_ty + 22; y++) {
            if (x >= 0 && x < WORLD_W && y >= 0 && y < WORLD_H)
                hw_map[(y & 31) * 32 + hw_x] = world_map[y][x];
        }
    }

    // Vertical Seam
    if (cur_ty > old_ty) { // Moved Down
        int y = cur_ty + 20;
        int hw_y = y & 31;
        for (int x = cur_tx - 1; x < cur_tx + 31; x++) {
            if (x >= 0 && x < WORLD_W && y >= 0 && y < WORLD_H)
                hw_map[hw_y * 32 + (x & 31)] = world_map[y][x];
        }
    } else if (cur_ty < old_ty) { // Moved Up
        int y = cur_ty - 1;
        int hw_y = y & 31;
        for (int x = cur_tx - 1; x < cur_tx + 31; x++) {
            if (x >= 0 && x < WORLD_W && y >= 0 && y < WORLD_H)
                hw_map[hw_y * 32 + (x & 31)] = world_map[y][x];
        }
    }
}

void update_map_seam(int prev_cx, int prev_cy, int cx, int cy); // Forward decl
int is_tile_solid(int tile); // Forward decl

void set_tile(int x, int y, unsigned short tile) {
    if (x < 0 || x >= WORLD_W || y < 0 || y >= WORLD_H) return;
    world_map[y][x] = tile;
    
    // Update collision LUT for this tile index if it's dynamic
    int tile_idx = tile & 0x3FF;
    if (tile_idx < 512) tile_solid_lut[tile_idx] = is_tile_solid(tile_idx);
    
    // Sync to hardware map ONLY if on-screen to avoid corrupting circular buffer.
    int tx = cam_x >> 3;
    int ty = cam_y >> 3;
    if (x >= tx && x <= tx + 31 && y >= ty && y <= ty + 21) {
        hw_map[(y & 31) * 32 + (x & 31)] = tile;
    }
}

int __attribute__((section(".iwram"))) is_tile_solid(int tile) {
    if (tile == TILE_AIR || tile == TILE_WOOD || tile == TILE_LEAVES || tile == TILE_TORCH) return 0;
    if (tile >= 11 && tile <= 15) return 0; // GrassPlants, (Lava Slot), Furnace, Workbench, Chest
    if (tile >= 18 && tile <= 27) return 0; // Furnace parts, Workbench R, Chest parts, Sapling
    if (tile >= 32 && tile <= 38) return 0; // Open Doors, Anvils, Chairs
    if (tile >= TILE_TABLE_TL && tile <= 511) return 0; // Table, Tree tops and other non-solid tiles
    return 1;
}

int __attribute__((section(".iwram"))) is_solid(int px, int py) {
    if (px < 0 || py < 0 || px >= (WORLD_W << 3) || py >= (WORLD_H << 3)) return 1;
    return tile_solid_lut[world_map[py >> 3][px >> 3] & 0x3FF];
}

int __attribute__((section(".iwram"))) check_collision(int x, int y) {
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

void save_game(void) {
    volatile uint8_t* sram = (volatile uint8_t*)0x0E000000;
    sram[0] = 'T'; sram[1] = 'E'; sram[2] = 'R'; sram[3] = 'R';
    int addr = 4;
    
    // Compress Map (RLE)
    uint16_t* map1d = (uint16_t*)world_map;
    uint16_t current_tile = map1d[0];
    int run_len = 0;
    for (int i = 0; i < WORLD_W * WORLD_H; i++) {
        if (map1d[i] == current_tile && run_len < 255) {
            run_len++;
        } else {
            sram[addr++] = current_tile & 0xFF;
            sram[addr++] = (current_tile >> 8) & 0xFF;
            sram[addr++] = run_len;
            current_tile = map1d[i];
            run_len = 1;
        }
    }
    sram[addr++] = current_tile & 0xFF;
    sram[addr++] = (current_tile >> 8) & 0xFF;
    sram[addr++] = run_len;
    
    #define WRITE_INT(val) do { sram[addr++] = (val) & 0xFF; sram[addr++] = ((val) >> 8) & 0xFF; sram[addr++] = ((val) >> 16) & 0xFF; sram[addr++] = ((val) >> 24) & 0xFF; } while(0)
    WRITE_INT(p_x); WRITE_INT(p_y); WRITE_INT(p_health); WRITE_INT(spawn_x); WRITE_INT(spawn_y);
    for(int i=0; i<12; i++) WRITE_INT(hotbar_id[i]);
    for(int i=0; i<12; i++) WRITE_INT(hotbar_count[i]);
    
    for(int i=0; i<MAX_CHESTS; i++) {
        WRITE_INT(chests[i].x); WRITE_INT(chests[i].y); WRITE_INT(chests[i].active);
        for(int j=0; j<12; j++) WRITE_INT(chests[i].items_id[j]);
        for(int j=0; j<12; j++) WRITE_INT(chests[i].items_count[j]);
    }
}

int load_game(void) {
    volatile uint8_t* sram = (volatile uint8_t*)0x0E000000;
    if (sram[0] != 'T' || sram[1] != 'E' || sram[2] != 'R' || sram[3] != 'R') return 0;
    int addr = 4;
    
    uint16_t* map1d = (uint16_t*)world_map;
    int i = 0;
    while (i < WORLD_W * WORLD_H) {
        uint16_t tile = sram[addr++];
        tile |= (sram[addr++] << 8);
        uint8_t count = sram[addr++];
        for (int c = 0; c < count && i < WORLD_W * WORLD_H; c++) {
            map1d[i++] = tile;
        }
    }
    
    #define READ_INT(val) do { val = sram[addr] | (sram[addr+1] << 8) | (sram[addr+2] << 16) | (sram[addr+3] << 24); addr+=4; } while(0)
    READ_INT(p_x); READ_INT(p_y); READ_INT(p_health); READ_INT(spawn_x); READ_INT(spawn_y);
    for(int k=0; k<12; k++) READ_INT(hotbar_id[k]);
    for(int k=0; k<12; k++) READ_INT(hotbar_count[k]);
    
    for(int k=0; k<MAX_CHESTS; k++) {
        READ_INT(chests[k].x); READ_INT(chests[k].y); READ_INT(chests[k].active);
        for(int j=0; j<12; j++) READ_INT(chests[k].items_id[j]);
        for(int j=0; j<12; j++) READ_INT(chests[k].items_count[j]);
    }
    return 1;
}

int main(void) {
    // 0. Show Title Screen
    int is_continue = run_title_screen();

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
    // Load Palette (DMA)
    dma_copy(tileset_palette, (void*)MEM_PAL_BG, 256, DMA_16BIT);
    // Backdrop color
    MEM_PAL_BG[0] = tileset_palette[0];
    // Load Tiles (DMA)
    dma_copy(tileset_graphics, (void*)MEM_BG_TILES, sizeof(tileset_graphics)/2, DMA_16BIT);
    
    // Generate a 64x64 circular light mask at tile 480 (uses 128 tile units for 256-color 1D mapping)
    // sprite_graphics is 15360 bytes = 480 blocks. So mask starts at block 480.
    // Address offset = 488 * 32 = 15616 bytes. In uint16_t, index is 15616 / 2 = 7808.
    volatile uint16_t* mask_dest = &MEM_OBJ_TILES[7808];
    for(int ty=0; ty<8; ty++) {
        for(int tx=0; tx<8; tx++) {
            for(int y=0; y<8; y++) {
                for(int x=0; x<8; x+=2) {
                    int px1 = tx*8+x, py1 = ty*8+y;
                    int dx1 = px1-32, dy1 = py1-32;
                    int v1 = (dx1*dx1 + dy1*dy1 < 30*30) ? 1 : 0; // Slightly smaller than 32 to ensure clear edges
                    int px2 = tx*8+x+1, py2 = ty*8+y;
                    int dx2 = px2-32, dy2 = py2-32;
                    int v2 = (dx2*dx2 + dy2*dy2 < 30*30) ? 1 : 0;
                    *mask_dest++ = (v2 << 8) | v1;
                }
            }
        }
    }
    
    // Init Tile Collision LUT
    for (int i = 0; i < 512; i++) tile_solid_lut[i] = is_tile_solid(i);
    
    for (int i = 0; i < 32 * 32; i++) bg_map[i] = 0; // Clear background to solid Sky Blue (Tile 0)
    
    // Clear map
    for (int i=0; i<32*32; i++) {
        hw_map[i] = TILE_AIR;
    }

    if (is_continue) {
        if (!load_game()) {
            is_continue = 0; // Fallback if loading fails
        }
    }
    
    if (!is_continue) {
        // Reset player for new game
        for (int i=0; i<12; i++) { hotbar_id[i] = 0; hotbar_count[i] = 0; }
        hotbar_id[0] = ITEM_SWORD; hotbar_count[0] = 1;
        hotbar_id[1] = ITEM_PICKAXE; hotbar_count[1] = 1;
        hotbar_id[2] = ITEM_AXE; hotbar_count[2] = 1;

        generate_world();
        
        p_x = ((WORLD_W * 8) / 2) << 8;
        p_y = 0;
        
        // Find surface
        for(int y=0; y<WORLD_H; y++){
            if(world_map[y][WORLD_W/2] != TILE_AIR) {
                p_y = ((y * 8) - 40) << 8;
                break;
            }
        }
        
        // Initialize chests from world
        for (int i = 0; i < MAX_CHESTS; i++) chests[i].active = 0;
        int chest_count = 0;
        for (int y = 0; y < WORLD_H - 1; y++) {
            for (int x = 0; x < WORLD_W - 1; x++) {
                if (world_map[y][x] == TILE_CHEST && chest_count < MAX_CHESTS) {
                    chests[chest_count].x = x;
                    chests[chest_count].y = y;
                    chests[chest_count].active = 1;
                    // Populating loot based on new probabilities (Rares 1/300, Regular 1/100)
                    for (int s = 0; s < 12; s++) {
                        chests[chest_count].items_id[s] = 0;
                        chests[chest_count].items_count[s] = 0;
                        if ((rand_next() % 300) == 0) { chests[chest_count].items_id[s] = ITEM_MAGIC_MIRROR; chests[chest_count].items_count[s] = 1; continue; }
                        if ((rand_next() % 300) == 0) { chests[chest_count].items_id[s] = ITEM_REGEN_BAND; chests[chest_count].items_count[s] = 1; continue; }
                        if ((rand_next() % 300) == 0) { chests[chest_count].items_id[s] = ITEM_CLOUD_BOTTLE; chests[chest_count].items_count[s] = 1; continue; }
                        if ((rand_next() % 300) == 0) { chests[chest_count].items_id[s] = ITEM_DEPTH_METER; chests[chest_count].items_count[s] = 1; continue; }
                        if ((rand_next() % 300) == 0) { chests[chest_count].items_id[s] = ITEM_ROCKET_BOOTS; chests[chest_count].items_count[s] = 1; continue; }
                        
                        if ((rand_next() % 100) == 0) { chests[chest_count].items_id[s] = ITEM_TORCH; chests[chest_count].items_count[s] = 5 + (rand_next() % 10); continue; }
                        if ((rand_next() % 100) == 0) { chests[chest_count].items_id[s] = ITEM_IRON_BAR; chests[chest_count].items_count[s] = 1 + (rand_next() % 3); continue; }
                        if ((rand_next() % 100) == 0) { chests[chest_count].items_id[s] = ITEM_COPPER_BAR; chests[chest_count].items_count[s] = 1 + (rand_next() % 3); continue; }
                        if ((rand_next() % 100) == 0) { chests[chest_count].items_id[s] = ITEM_GOLD_BAR; chests[chest_count].items_count[s] = 1 + (rand_next() % 3); continue; }
                    }
                    chest_count++;
                }
            }
        }
    }

    // Enable Objects, Windows 0 & 1, and OBJ Window
    REG_DISPCNT |= OBJ_ENABLE | OBJ_1D_MAP | WIN0_ENABLE | WIN1_ENABLE | WINOBJ_ENABLE;

    // Load Sprite Palette
    dma_copy(sprite_palette, (void*)MEM_PAL_OBJ, 256, DMA_16BIT);
    // Load Sprite Graphics
    dma_copy(sprite_graphics, (void*)MEM_OBJ_TILES, sizeof(sprite_graphics)/2, DMA_16BIT);
    
    int p_dx = 0;
    int p_dy = 0;
    int grounded = 0;

    int was_grounded = 1;
    int fall_peak_y = p_y;
    int can_double_jump = 0;
    int rocket_fuel = 0;
    int facing_left = 0;
    int frame_timer = 0;
    int current_frame = 0;
    
    spawn_x = p_x;
    spawn_y = p_y;
    
    int mining_mode = 0;
    int cursor_dx = 0;
    int cursor_dy = 0;
    
    int inv_mode = 0; // 0=Inventory, 1=Crafting
    int inv_cursor = 0; // 0-11 for inv, 0-2 for craft
    int inv_held_cursor = -1; // -1 = none
    int cursor_move_delay = 0;
    
    int active_slot = 0; // 0 = Slot 1, 1 = Slot 2, ...
    
    int swing_timer = 0;
    int swing_frame = 0;
    
    p_health = 100;
    int p_flicker = 0;
    int p_last_damage_timer = 600; // Start ready for regen
    int p_regen_timer = 0;
    
    Slime slimes[MAX_SLIMES];
    for(int i=0; i<MAX_SLIMES; i++) slimes[i].active = 0;
    
    // Slimes initialized above.
    // Chests initialized in is_continue block.
    
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
    audio_init();
    while (1) {
        vsync();
        update_day_night();
        REG_DISPCNT = MODE_0 | BG0_ENABLE | BG1_ENABLE | OBJ_ENABLE | OBJ_1D_MAP | WIN0_ENABLE | WIN1_ENABLE | WINOBJ_ENABLE;
        audio_update();
        
        prev_keys = curr_keys;
        curr_keys = REG_KEYINPUT;
        
        // Passive Item Check
        int has_regen = 0;
        int has_cloud = 0;
        int has_depth = 0;
        int has_rocket = 0;
        for (int i = 0; i < 24; i++) {
            int id = get_comb_id(i);
            if (id == ITEM_REGEN_BAND) has_regen = 1;
            else if (id == ITEM_CLOUD_BOTTLE) has_cloud = 1;
            else if (id == ITEM_DEPTH_METER) has_depth = 1;
            else if (id == ITEM_ROCKET_BOOTS) has_rocket = 1;
        }
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
        if (KEY_PRESSED(KEY_START)) {
            inv_open = !inv_open;
            if (!inv_open) {
                inv_mode = 0;
                inv_cursor = 0;
                inv_held_cursor = -1;
            }
        }
        
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
                if (inv_mode == 1) update_available_recipes(hotbar_id, hotbar_count);
            }

            // Inventory Navigation
            if (cursor_move_delay > 0) cursor_move_delay--;
            if (cursor_move_delay == 0) {
                if (inv_mode == 0 || inv_mode == 2) { // Inventory / Chest Move
                    int max_slots = (inv_mode == 2) ? 24 : 12;
                    if (KEY_DOWN(KEY_RIGHT)) { 
                        if (inv_mode == 2 && inv_cursor % 4 == 3 && inv_cursor < 12) inv_cursor += 9; // Jump to chest row? nah, just loop
                        else inv_cursor = (inv_cursor + 1) % max_slots; 
                        cursor_move_delay = 10; 
                    }
                    if (KEY_DOWN(KEY_LEFT))  { 
                        inv_cursor = (inv_cursor + max_slots - 1) % max_slots; 
                        cursor_move_delay = 10; 
                    }
                    if (KEY_DOWN(KEY_DOWN_BTN)) { inv_cursor = (inv_cursor + 4) % max_slots; cursor_move_delay = 10; }
                    if (KEY_DOWN(KEY_UP))    { inv_cursor = (inv_cursor + max_slots - 4) % max_slots; cursor_move_delay = 10; }
                } else { // Crafting Move
                    if (num_available > 0) {
                        if (KEY_DOWN(KEY_DOWN_BTN)) { inv_cursor = (inv_cursor + 1) % num_available; cursor_move_delay = 10; }
                        if (KEY_DOWN(KEY_UP))    { inv_cursor = (inv_cursor + num_available - 1) % num_available; cursor_move_delay = 10; }
                    }
                }
            }
            
            if (inv_mode == 1) { // Crafting Logic
                if (num_available > 0 && inv_cursor < num_available) {
                    if (KEY_PRESSED(KEY_A)) {
                        int r_idx = available_recipes[inv_cursor];
                        // Consume ingredients
                        for (int j = 0; j < 3; j++) {
                            if (recipes[r_idx].ing_id[j] > 0) {
                                int id = recipes[r_idx].ing_id[j];
                                int qty = recipes[r_idx].ing_qty[j];
                                for (int s = 0; s < 12; s++) {
                                    if (hotbar_id[s] == id) {
                                        int take = (hotbar_count[s] < qty) ? hotbar_count[s] : qty;
                                        hotbar_count[s] -= take; qty -= take;
                                        if (hotbar_count[s] == 0) hotbar_id[s] = 0;
                                        if (qty <= 0) break;
                                    }
                                }
                            }
                        }
                        // Add result
                        int res_id = recipes[r_idx].res_id;
                        int res_qty = recipes[r_idx].res_qty;
                        for (int q = 0; q < res_qty; q++) {
                            int added = 0;
                            for (int s = 0; s < 12; s++) {
                                if (hotbar_id[s] == res_id && hotbar_count[s] > 0 && hotbar_count[s] < 99) {
                                    hotbar_count[s]++; added = 1; break;
                                }
                            }
                            if (!added) {
                                for (int s = 0; s < 12; s++) {
                                    if (hotbar_count[s] == 0 && hotbar_id[s] != ITEM_SWORD && hotbar_id[s] != ITEM_PICKAXE && hotbar_id[s] != ITEM_AXE) {
                                        hotbar_id[s] = res_id; hotbar_count[s] = 1; added = 1; break;
                                    }
                                }
                            }
                        }
                        update_available_recipes(hotbar_id, hotbar_count);
                        if (inv_cursor >= num_available) inv_cursor = (num_available > 0) ? num_available - 1 : 0;
                    }
                }
            } else {
                // Storage Swap
                if (KEY_PRESSED(KEY_A)) {
                    if (inv_held_cursor == -1) {
                        // Pick up item (if slot not empty)
                        if (get_comb_count(inv_cursor) > 0 || (inv_cursor < 3 && inv_mode == 0)) { 
                            inv_held_cursor = inv_cursor;
                        }
                    } else {
                        // Swap or Merge
                        int tid = get_comb_id(inv_cursor);
                        int hid = get_comb_id(inv_held_cursor);
                        if (tid == hid && tid != 0 && tid != ITEM_SWORD && tid != ITEM_PICKAXE && tid != ITEM_AXE) {
                            int space = 99 - get_comb_count(inv_cursor);
                            int transfer = (get_comb_count(inv_held_cursor) < space) ? get_comb_count(inv_held_cursor) : space;
                            set_comb_count(inv_cursor, get_comb_count(inv_cursor) + transfer);
                            set_comb_count(inv_held_cursor, get_comb_count(inv_held_cursor) - transfer);
                            if (get_comb_count(inv_held_cursor) == 0) {
                                set_comb_id(inv_held_cursor, 0);
                                inv_held_cursor = -1;
                            }
                        } else {
                            int temp_id = get_comb_id(inv_cursor);
                            int temp_count = get_comb_count(inv_cursor);
                            set_comb_id(inv_cursor, get_comb_id(inv_held_cursor));
                            set_comb_count(inv_cursor, get_comb_count(inv_held_cursor));
                            set_comb_id(inv_held_cursor, temp_id);
                            set_comb_count(inv_held_cursor, temp_count);
                            inv_held_cursor = -1;
                        }
                        update_available_recipes(hotbar_id, hotbar_count);
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
                            int amt = 1;
                            int acorns = 0;

                            // Basic block mappings
                            if (target == TILE_GRASS || target == TILE_DIRT) drop = ITEM_DIRT;
                            else if (target == TILE_STONE) drop = ITEM_STONE;
                            else if (target == TILE_ASH) drop = ITEM_ASH;
                            else if (target == TILE_COPPER) drop = ITEM_COPPER_ORE;
                            else if (target == TILE_IRON) drop = ITEM_IRON_ORE;
                            else if (target == TILE_GOLD) drop = ITEM_GOLD_ORE;
                            else if (target == TILE_TORCH) drop = ITEM_TORCH;
                            else if (target == TILE_MUD) drop = ITEM_MUD;
                            else if (target == TILE_JUNGLE_GRASS) drop = ITEM_JUNGLE_GRASS;
                            else if (target == TILE_PLANKS) drop = ITEM_PLANKS;
                            else if (target == TILE_SAPLING) { drop = 0; amt = 0; }

                            // Multi-tile object macros
                            #define IS_FURNACE(t) (t == TILE_FURNACE || (t >= TILE_FURNACE_TM && t <= TILE_FURNACE_BR))
                            #define IS_WORKBENCH(t) (t == TILE_WORKBENCH || t == TILE_WORKBENCH_R)
                            #define IS_CHEST(t) (t == TILE_CHEST || (t >= TILE_CHEST_TR && t <= TILE_CHEST_BR))

                            if (target == TILE_WOOD) {
                                if (cur_id == ITEM_AXE) {
                                    drop = ITEM_PLANKS; amt = 0; acorns = (seed % 3);
                                    int yy = cy;
                                    while (yy + 1 < WORLD_H && world_map[yy + 1][cx] == TILE_WOOD) yy++;
                                    while (yy >= 0) {
                                        int t_center = world_map[yy][cx];
                                        if (t_center == TILE_WOOD || t_center == TILE_LEAVES || (t_center >= 28 && t_center <= 126)) {
                                            for (int lx = cx - 2; lx <= cx + 2; lx++) {
                                                if (lx >= 0 && lx < WORLD_W) {
                                                    int t = world_map[yy][lx];
                                                    if (t == TILE_WOOD || t == TILE_LEAVES || (t >= 28 && t <= 126)) set_tile(lx, yy, TILE_AIR);
                                                }
                                            }
                                            amt++; yy--;
                                        } else break;
                                    }
                                } else { amt = 0; }
                            } else if (target == TILE_LEAVES || (target >= 39 && target <= 137)) {
                                if (cur_id == ITEM_AXE) set_tile(cx, cy, TILE_AIR);
                                amt = 0;
                            } else if (cur_id == ITEM_PICKAXE) {
                                if (IS_WORKBENCH(target)) {
                                    drop = ITEM_WORKBENCH; amt = 1;
                                    int ox = cx; if (target == TILE_WORKBENCH_R) ox--;
                                    for(int x=ox; x<=ox+1; x++) if (IS_WORKBENCH(world_map[cy][x])) set_tile(x, cy, TILE_AIR);
                                } else if (IS_FURNACE(target)) {
                                    drop = ITEM_FURNACE; amt = 1;
                                    int ox = cx, oy = cy;
                                    if (target == TILE_FURNACE_TM || target == TILE_FURNACE_BM) ox--;
                                    else if (target == TILE_FURNACE_TR || target == TILE_FURNACE_BR) ox -= 2;
                                    if (target >= TILE_FURNACE_BL && target <= TILE_FURNACE_BR) oy--;
                                    for(int y0=oy; y0<=oy+1; y0++) for(int x0=ox; x0<=ox+2; x0++)
                                        if (IS_FURNACE(world_map[y0][x0])) set_tile(x0, y0, TILE_AIR);
                                } else if (IS_CHEST(target)) {
                                    drop = ITEM_CHEST; amt = 1; remove_chest(cx, cy);
                                    int ox = cx, oy = cy;
                                    if (target == TILE_CHEST_TR || target == TILE_CHEST_BR) ox--;
                                    if (target == TILE_CHEST_BL || target == TILE_CHEST_BR) oy--;
                                    for(int y0=oy; y0<=oy+1; y0++) for(int x0=ox; x0<=ox+1; x0++)
                                        if (IS_CHEST(world_map[y0][x0])) set_tile(x0, y0, TILE_AIR);
                                } else if ((target & 0x3FF) >= TILE_DOOR_T && (target & 0x3FF) <= TILE_DOOR_OPEN_B) {
                                    drop = ITEM_DOOR; amt = 1;
                                    int y_base = cy;
                                    int clean_target = target & 0x3FF;
                                    if (clean_target == TILE_DOOR_B || clean_target == TILE_DOOR_OPEN_B) y_base -= 2; 
                                    else if (clean_target == TILE_DOOR_M || clean_target == TILE_DOOR_OPEN_M) y_base -= 1;
                                    for(int dy=0; dy<3; dy++) if (y_base+dy >= 0 && y_base+dy < WORLD_H) set_tile(cx, y_base+dy, TILE_AIR);
                                } else if (target == TILE_ANVIL_L || target == TILE_ANVIL_R) {
                                    drop = ITEM_ANVIL; amt = 1;
                                    int x_base = cx; if (target == TILE_ANVIL_R) x_base--;
                                    set_tile(x_base, cy, TILE_AIR); set_tile(x_base+1, cy, TILE_AIR);
                                } else if (target == TILE_CHAIR_T || target == TILE_CHAIR_B) {
                                    drop = ITEM_CHAIR; amt = 1;
                                    int y_base = cy; if (target == TILE_CHAIR_B) y_base--;
                                    set_tile(cx, y_base, TILE_AIR); set_tile(cx, y_base+1, TILE_AIR);
                                } else {
                                    set_tile(cx, cy, TILE_AIR);
                                }
                            } else { amt = 0; }

                            // Grant Items
                            for (int a = 0; a < amt + acorns; a++) {
                                int id = (a < amt) ? drop : ITEM_ACORN;
                                if (id <= 0) continue;
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
                                    set_tile(cx, cy, TILE_SAPLING);
                                    hotbar_count[active_slot]--;
                                    if (hotbar_count[active_slot] <= 0) {
                                        hotbar_count[active_slot] = 0;
                                        hotbar_id[active_slot] = 0;
                                    }
                                    swing_timer = 15;
                                }
                            }
                        }
                    } else if (cur_id != ITEM_PICKAXE && cur_id != ITEM_AXE && cur_id != ITEM_ACORN && cur_count > 0 && swing_timer == 0 && cur_id != ITEM_GEL && cur_id != ITEM_COPPER_BAR && cur_id != ITEM_IRON_BAR && cur_id != ITEM_GOLD_BAR && cur_id != ITEM_SWORD) {
                        if (!is_solid(cx * 8, cy * 8)) {
                            int tile_to_place = 0;
                            if (cur_id == ITEM_DIRT) tile_to_place = TILE_DIRT;
                            else if (cur_id == ITEM_STONE) tile_to_place = TILE_STONE;
                            else if (cur_id == ITEM_ASH) tile_to_place = TILE_ASH;
                            else if (cur_id == ITEM_PLANKS) tile_to_place = TILE_PLANKS;
                            else if (cur_id == ITEM_MUD) tile_to_place = TILE_MUD;
                            else if (cur_id == ITEM_JUNGLE_GRASS) tile_to_place = TILE_JUNGLE_GRASS;
                            else if (cur_id == ITEM_TORCH) tile_to_place = TILE_TORCH;
                            else if (cur_id == ITEM_COPPER_ORE) tile_to_place = TILE_COPPER;
                            else if (cur_id == ITEM_IRON_ORE) tile_to_place = TILE_IRON;
                            else if (cur_id == ITEM_GOLD_ORE) tile_to_place = TILE_GOLD;
                            int placed = 0;
                            if (cur_id == ITEM_WORKBENCH) {
                                if (cx < WORLD_W - 1 && !is_tile_solid(world_map[cy][cx+1])) {
                                    set_tile(cx, cy, TILE_WORKBENCH); set_tile(cx+1, cy, TILE_WORKBENCH_R); placed = 1;
                                }
                            } else if (cur_id == ITEM_FURNACE) {
                                if (cx < WORLD_W - 2 && cy < WORLD_H - 1 && 
                                    !is_tile_solid(world_map[cy][cx+1]) && !is_tile_solid(world_map[cy][cx+2]) &&
                                    !is_tile_solid(world_map[cy+1][cx]) && !is_tile_solid(world_map[cy+1][cx+1]) && !is_tile_solid(world_map[cy+1][cx+2])) {
                                    set_tile(cx, cy, TILE_FURNACE); set_tile(cx+1, cy, TILE_FURNACE_TM); set_tile(cx+2, cy, TILE_FURNACE_TR);
                                    set_tile(cx, cy+1, TILE_FURNACE_BL); set_tile(cx+1, cy+1, TILE_FURNACE_BM); set_tile(cx+2, cy+1, TILE_FURNACE_BR); placed = 1;
                                }
                            } else if (cur_id == ITEM_CHEST) {
                                if (cx < WORLD_W - 1 && cy < WORLD_H - 1 && 
                                    !is_tile_solid(world_map[cy][cx+1]) && !is_tile_solid(world_map[cy+1][cx]) && !is_tile_solid(world_map[cy+1][cx+1])) {
                                    set_tile(cx, cy, TILE_CHEST); set_tile(cx+1, cy, TILE_CHEST_TR);
                                    set_tile(cx, cy+1, TILE_CHEST_BL); set_tile(cx+1, cy+1, TILE_CHEST_BR);
                                    add_chest(cx, cy); placed = 1;
                                }
                            } else if (cur_id == ITEM_ANVIL) {
                                if (cx < WORLD_W - 1 && !is_tile_solid(world_map[cy][cx+1])) {
                                    set_tile(cx, cy, TILE_ANVIL_L); set_tile(cx+1, cy, TILE_ANVIL_R); placed = 1;
                                }
                            } else if (cur_id == ITEM_CHAIR) {
                                if (cy > 0 && !is_tile_solid(world_map[cy-1][cx])) {
                                    set_tile(cx, cy, TILE_CHAIR_B); set_tile(cx, cy-1, TILE_CHAIR_T); placed = 1;
                                }
                            } else if (cur_id == ITEM_DOOR) {
                                if (cy > 1 && cy < WORLD_H - 1 && 
                                    !is_tile_solid(world_map[cy-1][cx]) && !is_tile_solid(world_map[cy-2][cx]) &&
                                    is_tile_solid(world_map[cy+1][cx]) && is_tile_solid(world_map[cy-3][cx])) {
                                    set_tile(cx, cy, TILE_DOOR_B); set_tile(cx, cy-1, TILE_DOOR_M); set_tile(cx, cy-2, TILE_DOOR_T); placed = 1;
                                }
                            } 
                            if (tile_to_place > 0) { set_tile(cx, cy, tile_to_place); placed = 1; }
                            
                            if (placed) {
                                hotbar_count[active_slot]--;
                                if (hotbar_count[active_slot] <= 0) {
                                    hotbar_count[active_slot] = 0;
                                    hotbar_id[active_slot] = 0;
                                }
                                swing_timer = 10;
                            }
                        }
                    }
                }
            }
            
        } else {
            // Move Player
            int ix = p_x >> 8;
            int iy = p_y >> 8;
            int px_tile = ix / 8;
            int py_tile = iy / 8;
            cursor_dx = 0;
            cursor_dy = 0;
            if (KEY_DOWN(KEY_RIGHT)) { p_dx = 384; facing_left = 0; }
            if (KEY_DOWN(KEY_LEFT))  { p_dx = -384; facing_left = 1; }
            
            // Smart Door Opening (Walk into door to open)
            if (p_dx != 0) {
                // Check in front of the hitbox edges (10 left, 21 right) - increased lookahead to 4px
                int tx = (p_dx < 0) ? (ix + 10 - 4) / 8 : (ix + 21 + 4) / 8;
                // Check vertical range from head (7) to feet (29)
                int ty_top = (iy + 4) / 8; // Slightly more generous top check
                int ty_bot = (iy + 30) / 8; // slightly more generous bottom check
                
                if (tx >= 0 && tx < WORLD_W) {
                    for (int rty = ty_top; rty <= ty_bot; rty++) {
                        if (rty >= 0 && rty < WORLD_H) {
                            int t = world_map[rty][tx] & 0x3FF;
                            if (t >= TILE_DOOR_T && t <= TILE_DOOR_B) { 
                                int y_base = rty;
                                if (t == TILE_DOOR_B) y_base -= 2;
                                else if (t == TILE_DOOR_M) y_base -= 1;
                                
                                int slab_x = (p_dx > 0) ? tx + 1 : tx - 1;
                                int flip_flag = (p_dx < 0) ? 0x0400 : 0;
                                // Open door: Hinge at tx, Slab at slab_x
                                set_tile(tx, y_base, TILE_DOOR_OPEN_T | flip_flag);
                                set_tile(tx, y_base+1, TILE_DOOR_OPEN_M | flip_flag);
                                set_tile(tx, y_base+2, TILE_DOOR_OPEN_B | flip_flag);
                                if (slab_x >= 0 && slab_x < WORLD_W) {
                                    set_tile(slab_x, y_base, TILE_DOOR_SLAB_T | flip_flag);
                                    set_tile(slab_x, y_base+1, TILE_DOOR_SLAB_M | flip_flag);
                                    set_tile(slab_x, y_base+2, TILE_DOOR_SLAB_B | flip_flag);
                                }
                                audio_play_sfx(sfx_dig0, sfx_dig0_len);
                                break;
                            }
                        }
                    }
                }
            }

            // Smart Door Closing (Auto-Close when away)
            int p_left_t  = (ix + 10) / 8;
            int p_right_t = (ix + 21) / 8;
            for (int dy = -3; dy <= 3; dy++) {
                for (int dx = -4; dx <= 4; dx++) {
                    int ctx = px_tile + dx;
                    int cty = py_tile + dy;
                    if (ctx >= 0 && ctx < WORLD_W && cty >= 0 && cty < WORLD_H) {
                        int t = world_map[cty][ctx] & 0x3FF;
                        if (t == TILE_DOOR_OPEN_T || t == TILE_DOOR_OPEN_M || t == TILE_DOOR_OPEN_B) {
                            // Close if player is not in the column AND not in adjacent columns
                            if (ctx < p_left_t - 1 || ctx > p_right_t + 1) {
                                int y_base = cty;
                                if (t == TILE_DOOR_OPEN_B || t == TILE_DOOR_SLAB_B) y_base -= 2;
                                else if (t == TILE_DOOR_OPEN_M || t == TILE_DOOR_SLAB_M) y_base -= 1;
                                
                                // To close properly, we need to find which column is the hinge.
                                // Simplification: Doors always open 2-wide. If we find an OPEN part, 
                                // check neighbors to find the hinge or slab.
                                // For now, just clear the column and its immediate neighbors if they are Slab.
                                set_tile(ctx, y_base, TILE_DOOR_T);
                                set_tile(ctx, y_base+1, TILE_DOOR_M);
                                set_tile(ctx, y_base+2, TILE_DOOR_B);
                                
                                // Scan left and right for slabs to remove
                                for (int sx = ctx - 1; sx <= ctx + 1; sx += 2) {
                                    if (sx >= 0 && sx < WORLD_W) {
                                        int st = world_map[y_base][sx] & 0x3FF;
                                        if (st == TILE_DOOR_SLAB_T) {
                                            set_tile(sx, y_base, TILE_AIR);
                                            set_tile(sx, y_base+1, TILE_AIR);
                                            set_tile(sx, y_base+2, TILE_AIR);
                                        }
                                    }
                                }
                                audio_play_sfx(sfx_dig0, sfx_dig0_len); // Reuse dig sound for now
                            }
                        }
                    }
                }
            }
            
            if (grounded) {
                can_double_jump = 1;
                rocket_fuel = 60;
            }

            if (KEY_PRESSED(KEY_UP)) {
                if (grounded) {
                    p_dy = -800; // Jump
                    grounded = 0;
                } else if (has_cloud && can_double_jump) {
                    p_dy = -800; // Double Jump
                    can_double_jump = 0;
                    audio_play_sfx(sfx_swing, sfx_swing_len); // Sound feedback
                }
            }
            
            // Rocket Boots Flight
            if (KEY_DOWN(KEY_UP) && !grounded && has_rocket && rocket_fuel > 0) {
                p_dy = -400; // Anti-gravity thrust
                rocket_fuel--;
                if (frame_timer % 4 == 0) audio_play_sfx(sfx_swing, sfx_swing_len);
            }
            
            // Magic Mirror Use
            if (hotbar_id[active_slot] == ITEM_MAGIC_MIRROR && KEY_DOWN(KEY_B) && swing_timer == 0) {
                p_x = spawn_x;
                p_y = spawn_y;
                swing_timer = 30; // Cooldown
                audio_play_sfx(sfx_swing, sfx_swing_len);
            }
            
            if (KEY_DOWN(KEY_B) && swing_timer == 0) {
                swing_timer = 15;
                audio_play_sfx(sfx_swing, sfx_swing_len);
            }
            if (KEY_PRESSED(KEY_A) && !inv_open) {
                // Find the CLOSEST chest within a reasonable radius
                int ctx = (p_x >> 8) / 8 + 1; // Center of player (tile units)
                int cty = (p_y >> 8) / 8 + 2; 
                int cid = -1;
                int min_dist = 36; // Search radius: 6 tiles squared
                
                for (int i = 0; i < MAX_CHESTS; i++) {
                    if (!chests[i].active) continue;
                    // Compare distances to player center
                    int dx = chests[i].x - ctx;
                    int dy = chests[i].y - cty;
                    int dist = dx*dx + dy*dy;
                    if (dist < min_dist) {
                        min_dist = dist;
                        cid = i;
                    }
                }
                
                if (cid != -1) {
                    inv_open = 1;
                    inv_mode = 2;
                    open_chest_id = cid;
                    inv_cursor = 0;
                }
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
                        if (smart_cx == -1 && py_tile >= 0 && py_tile < WORLD_H) {
                            int t = world_map[py_tile][tx];
                            if (t != TILE_AIR && t != TILE_WOOD && t != TILE_LEAVES && (is_tile_solid(t) || (t >= 11 && t <= 38))) {
                                smart_cx = tx; smart_cy = py_tile; break;
                            }
                        }
                        if (smart_cx == -1 && py_tile - 1 >= 0) {
                            int t = world_map[py_tile-1][tx];
                            if (t != TILE_AIR && t != TILE_WOOD && t != TILE_LEAVES && (is_tile_solid(t) || (t >= 11 && t <= 38))) {
                                smart_cx = tx; smart_cy = py_tile - 1; break;
                            }
                        }
                        if (smart_cx == -1 && py_tile + 1 < WORLD_H) {
                            int t = world_map[py_tile+1][tx];
                            if (t != TILE_AIR && t != TILE_WOOD && t != TILE_LEAVES && (is_tile_solid(t) || (t >= 11 && t <= 38))) {
                                smart_cx = tx; smart_cy = py_tile + 1; break;
                            }
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
            
        // Helper macros for multi-tile checks
        #define IS_FURNACE(t) (t == TILE_FURNACE || (t >= TILE_FURNACE_TM && t <= TILE_FURNACE_BR))
        #define IS_WORKBENCH(t) (t == TILE_WORKBENCH || t == TILE_WORKBENCH_R)
        #define IS_CHEST(t) (t == TILE_CHEST || (t >= TILE_CHEST_TR && t <= TILE_CHEST_BR))
        #define IS_TABLE(t) (t >= TILE_TABLE_TL && t <= TILE_TABLE_BR)

        if (swing_timer == 14 && smart_cx != -1 && smart_cy != -1) {
            int rnd = seed % 3;
            if (rnd == 0) audio_play_sfx(sfx_dig0, sfx_dig0_len);
            else if (rnd == 1) audio_play_sfx(sfx_dig1, sfx_dig1_len);
            else audio_play_sfx(sfx_dig2, sfx_dig2_len);
            
            int target = world_map[smart_cy][smart_cx];
            int drop = 0;
            int amt = 0;
            int acorns = 0;

            // Safe tile clear (updates HW map ONLY if on-screen)
            #define CLEAR_TILE(tx, ty) set_tile(tx, ty, TILE_AIR)

            if (target == TILE_GRASS || target == TILE_DIRT) { drop = ITEM_DIRT; amt = 1; }
            else if (target == TILE_STONE) { drop = ITEM_STONE; amt = 1; }
            else if (target == TILE_ASH) { drop = ITEM_ASH; amt = 1; }
            else if (target == TILE_COPPER) { drop = ITEM_COPPER_ORE; amt = 1; }
            else if (target == TILE_IRON) { drop = ITEM_IRON_ORE; amt = 1; }
            else if (target == TILE_GOLD) { drop = ITEM_GOLD_ORE; amt = 1; }
            else if (target == TILE_TORCH) { drop = ITEM_TORCH; amt = 1; }
            else if (target == TILE_MUD) { drop = ITEM_MUD; amt = 1; }
            else if (target == TILE_JUNGLE_GRASS) { drop = ITEM_JUNGLE_GRASS; amt = 1; }
            else if (target == TILE_PLANKS) { drop = ITEM_PLANKS; amt = 1; }
            else if (IS_WORKBENCH(target)) {
                drop = ITEM_WORKBENCH; amt = 1;
                int ox = smart_cx; if (target == TILE_WORKBENCH_R) ox--;
                for(int x=ox; x<=ox+1; x++) if (IS_WORKBENCH(world_map[smart_cy][x])) CLEAR_TILE(x, smart_cy);
            } else if (IS_FURNACE(target)) {
                drop = ITEM_FURNACE; amt = 1;
                int ox = smart_cx, oy = smart_cy;
                if (target == TILE_FURNACE_TM || target == TILE_FURNACE_BM) ox--;
                else if (target == TILE_FURNACE_TR || target == TILE_FURNACE_BR) ox -= 2;
                if (target >= TILE_FURNACE_BL && target <= TILE_FURNACE_BR) oy--;
                for(int y=oy; y<=oy+1; y++) for(int x=ox; x<=ox+2; x++)
                    if (IS_FURNACE(world_map[y][x])) CLEAR_TILE(x, y);
            } else if (IS_CHEST(target)) {
                drop = ITEM_CHEST; amt = 1; remove_chest(smart_cx, smart_cy);
                int ox = smart_cx, oy = smart_cy;
                if (target == TILE_CHEST_TR || target == TILE_CHEST_BR) ox--;
                if (target == TILE_CHEST_BL || target == TILE_CHEST_BR) oy--;
                for(int y=oy; y<=oy+1; y++) for(int x=ox; x<=ox+1; x++)
                    if (IS_CHEST(world_map[y][x])) CLEAR_TILE(x, y);
            } else if ((target & 0x3FF) >= TILE_DOOR_T && (target & 0x3FF) <= TILE_DOOR_OPEN_B) {
                drop = ITEM_DOOR; amt = 1;
                int y_base = smart_cy;
                int clean_target = target & 0x3FF;
                if (clean_target == TILE_DOOR_B || clean_target == TILE_DOOR_OPEN_B) y_base -= 2; 
                else if (clean_target == TILE_DOOR_M || clean_target == TILE_DOOR_OPEN_M) y_base -= 1;
                for(int dy=0; dy<3; dy++) if (y_base+dy >=0 && y_base+dy < WORLD_H) CLEAR_TILE(smart_cx, y_base+dy);
            } else if (target == TILE_ANVIL_L || target == TILE_ANVIL_R) {
                drop = ITEM_ANVIL; amt = 1;
                int x_base = smart_cx; if (target == TILE_ANVIL_R) x_base--;
                CLEAR_TILE(x_base, smart_cy); CLEAR_TILE(x_base+1, smart_cy);
            } else if (target == TILE_CHAIR_T || target == TILE_CHAIR_B) {
                drop = ITEM_CHAIR; amt = 1;
                int y_base = smart_cy; if (target == TILE_CHAIR_B) y_base--;
                CLEAR_TILE(smart_cx, y_base); CLEAR_TILE(smart_cx, y_base+1);
            } else if (IS_TABLE(target)) {
                drop = ITEM_TABLE; amt = 1;
                int ox = smart_cx, oy = smart_cy;
                if (target == TILE_TABLE_TM || target == TILE_TABLE_BM) ox--;
                else if (target == TILE_TABLE_TR || target == TILE_TABLE_BR) ox -= 2;
                if (target >= TILE_TABLE_BL && target <= TILE_TABLE_BR) oy--;
                for(int y=oy; y<=oy+1; y++) for(int x=ox; x<=ox+2; x++)
                    if (IS_TABLE(world_map[y][x])) CLEAR_TILE(x, y);
            } else if (target == TILE_WOOD) {
                if (cur_tool == ITEM_AXE) {
                    drop = ITEM_PLANKS; amt = 0; acorns = (seed % 3);
                    int ty = smart_cy;
                    while (ty >= 0) {
                        int t_center = world_map[ty][smart_cx];
                        if (t_center == TILE_WOOD || t_center == TILE_LEAVES || (t_center >= TILE_FOLIAGE_START && t_center <= TILE_FOLIAGE_END)) {
                            CLEAR_TILE(smart_cx, ty);
                            for (int lx = smart_cx - 2; lx <= smart_cx + 2; lx++)
                                if (lx >= 0 && lx < WORLD_W) {
                                    int t = world_map[ty][lx];
                                    if (t == TILE_WOOD || t == TILE_LEAVES || (t >= TILE_FOLIAGE_START && t <= TILE_FOLIAGE_END)) CLEAR_TILE(lx, ty);
                                }
                            amt++; ty--;
                        } else break;
                    }
                }
            } else if (target != TILE_SAPLING && target != TILE_AIR && target != TILE_GRASS_PLANTS && target != TILE_LEAVES && target <= TILE_TABLE_BR) {
                if (cur_tool == ITEM_PICKAXE) { drop = target; amt = 1; }
            }

            // Final single-tile clear for basic blocks (Dirt, Stone, etc.)
            if (amt > 0 && world_map[smart_cy][smart_cx] != TILE_AIR && cur_tool == ITEM_PICKAXE) {
                CLEAR_TILE(smart_cx, smart_cy);
            }

            // Grant Items
            for (int a = 0; a < amt + acorns; a++) {
                int id = (a < amt) ? drop : ITEM_ACORN;
                if (id <= 0) continue;
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
        
        // --- Fall Damage Logic ---
        if (!grounded) {
            if (p_y < fall_peak_y) fall_peak_y = p_y; // Keep track of highest point
        }
        if (grounded && !was_grounded) {
            int fall_blocks = (p_y - fall_peak_y) >> 11; // 1 block = 8px * 256 sub = 2048 (1<<11)
            if (fall_blocks > 10) {
                int extra = fall_blocks - 10;
                int dmg = 5 + (extra * extra); // 5 base + quadratic growth
                p_health -= dmg;
                if (p_health < 0) p_health = 0;
                p_flicker = 45;
                p_last_damage_timer = 0;
                audio_play_sfx(sfx_hurt, sfx_hurt_len);
            }
        }
        if (p_dy < 0 || grounded) fall_peak_y = p_y; // Reset peak if moving up or on ground
        was_grounded = grounded;
        
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
            // Much lower spawn frequency (1 in 5000 frames)
            if ((rand_next() % 5000) == 0) { 
                for (int i = 0; i < MAX_SLIMES; i++) {
                    if (!slimes[i].active) {
                        int rx_off = (rand_next() % 240) - 120;
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
                            slimes[i].jump_timer = 60 + (rand_next() % 60);
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
                        slimes[i].dy = -500 - (rand_next() % 150);
                        slimes[i].dx = (p_sx > sx) ? 256 : -256;
                        // If stuck, jump straight UP to try clearing blocks
                        if (slimes[i].stuck_timer > 300) {
                            slimes[i].dy = -800; // Big jump up
                        }
                        slimes[i].jump_timer = 90 + (rand_next() % 60); // Jump much less often
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
                        if (p_health > 0) {
                            int damage = (slimes[i].type == 1) ? 7 : 5; // Blue=7, Green=5
                            p_health -= damage;
                            if (p_health < 0) p_health = 0;
                            audio_play_sfx(sfx_hurt, sfx_hurt_len);
                            p_last_damage_timer = 0; // Reset regen timer
                        }
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
                        slimes[i].hp -= (10 + (rand_next() % 3)); // 10-12 damage
                        slimes[i].flicker = 10;
                        audio_play_sfx(sfx_hit, sfx_hit_len);
                        slimes[i].dy = -300;
                        slimes[i].dx = (p_sx > sx) ? -512 : 512;
                        if (slimes[i].hp <= 0) {
                            slimes[i].active = 0;
                            int gel_amt = rand_next() % 4; // 0-3
                            for(int g=0; g<gel_amt; g++) {
                                for(int s=0; s<12; s++) {
                                    if(hotbar_id[s] == ITEM_GEL) { 
                                        if(hotbar_count[s] < 99) { 
                                            hotbar_count[s]++; 
                                            audio_play_sfx(sfx_grab, sfx_grab_len);
                                            break; 
                                        } 
                                    }
                                    else if(hotbar_count[s] == 0 && hotbar_id[s] != ITEM_SWORD && hotbar_id[s] != ITEM_PICKAXE && hotbar_id[s] != ITEM_AXE) {
                                        hotbar_id[s] = ITEM_GEL; hotbar_count[s] = 1; 
                                        audio_play_sfx(sfx_grab, sfx_grab_len);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if (p_flicker > 0) p_flicker--;

        // Health Regeneration
        if (p_last_damage_timer < 600) {
            p_last_damage_timer++;
        } else if (p_health < 100 && p_health > 0) {
            p_regen_timer++;
            int speed = has_regen ? 15 : 30; // 2x speed with band
            if (p_regen_timer >= speed) { 
                p_health++;
                p_regen_timer = 0;
            }
        }

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
            oam[1].attr2 = 56; 
        } else {
            oam[1].attr0 = 0x0200;
        }

        // Draw Health Bar (5 Hearts, 20HP each)
        for (int i = 0; i < 5; i++) {
            int h_x = 240 - 20 - (i * 14);
            int h_y = 6;
            int heart_hp = p_health - (i * 20);
            int heart_tile = -1;
            
            if (heart_hp >= 20) heart_tile = 32;      // Full
            else if (heart_hp >= 10) heart_tile = 40; // Half
            else if (heart_hp > 0)   heart_tile = 48; // Empty/Outline
            
            if (heart_tile != -1 && !inv_open) {
                oam[i + 2].attr0 = (h_y & 0x00FF) | 0x2000;
                oam[i + 2].attr1 = (h_x & 0x01FF) | 0x4000; 
                oam[i + 2].attr2 = heart_tile;
            } else oam[i + 2].attr0 = 0x0200;
        }
        
        // Draw Inventory / Hotbar Grid
        if (inv_mode == 0 || inv_mode == 2) { 
            int num_slots = (inv_mode == 2) ? 24 : (inv_open ? 12 : 4);
            for (int i = 0; i < 24; i++) {
                if (i >= num_slots) {
                    oam[i + 80].attr0 = 0x0200; oam[i + 104].attr0 = 0x0200;
                    oam[i*2 + 32].attr0 = 0x0200; oam[i*2 + 33].attr0 = 0x0200;
                    continue;
                }
                int bx, by;
                if (i < 12) { bx = 4 + (i % 4) * 28; by = 4 + (i / 4) * 28; }
                else { bx = 124 + ((i-12) % 4) * 28; by = 4 + ((i-12) / 4) * 28; }
                
                // 1. Background
                oam[i + 104].attr0 = (by & 0x00FF) | 0x2000;
                oam[i + 104].attr1 = (bx & 0x01FF) | 0x8000;
                oam[i + 104].attr2 = 58;

                // 2. Item Icon
                int id = get_comb_id(i);
                int count = get_comb_count(i);
                int item_base = 0;
                if (id == ITEM_SWORD) item_base = 122;
                else if (id == ITEM_PICKAXE) item_base = 138;
                else if (id == ITEM_AXE) item_base = 154;
                else if (id == ITEM_TORCH || id == TILE_TORCH) item_base = 170;
                else if (count > 0) {
                    if (id == ITEM_DIRT) item_base = 186; 
                    else if (id == ITEM_STONE) item_base = 194; 
                    else if (id == TILE_WOOD) item_base = 202; 
                    else if (id == TILE_PLANKS || id == ITEM_PLANKS) item_base = 210; 
                    else if (id == TILE_MUD || id == ITEM_MUD) item_base = 218;
                    else if (id == TILE_JUNGLE_GRASS || id == ITEM_JUNGLE_GRASS) item_base = 226;
                    else if (id == ITEM_ACORN) item_base = 234;
                    else if (id == ITEM_ASH) item_base = 242;
                    else if (id == ITEM_GEL) item_base = 282;
                    else if (id == ITEM_COPPER_ORE) item_base = 290;
                    else if (id == ITEM_IRON_ORE) item_base = 298;
                    else if (id == ITEM_COPPER_BAR) item_base = 306;
                    else if (id == ITEM_IRON_BAR) item_base = 314;
                    else if (id == ITEM_GOLD_ORE) item_base = 386;
                    else if (id == ITEM_GOLD_BAR) item_base = 394;
                    else if (id == ITEM_DOOR) item_base = 402;
                    else if (id == ITEM_ANVIL) item_base = 410;
                    else if (id == ITEM_CHAIR) item_base = 418;
                    else if (id == ITEM_TABLE) item_base = 434;
                    else if (id == ITEM_WORKBENCH) item_base = 322;
                    else if (id == ITEM_FURNACE) item_base = 330;
                    else if (id == ITEM_CHEST) item_base = 338;
                    else if (id == ITEM_REGEN_BAND) item_base = 346;
                    else if (id == ITEM_MAGIC_MIRROR) item_base = 354;
                    else if (id == ITEM_CLOUD_BOTTLE) item_base = 362;
                    else if (id == ITEM_DEPTH_METER) item_base = 370;
                    else if (id == ITEM_ROCKET_BOOTS) item_base = 378;
                }
                
                if (item_base > 0) {
                    oam[i + 80].attr0 = ((by + 8) & 0x00FF) | 0x2000;
                    oam[i + 80].attr1 = ((bx + 8) & 0x01FF) | 0x4000; 
                    oam[i + 80].attr2 = item_base;
                } else oam[i + 80].attr0 = 0x0200;

                // 3. Numbers
                if (id != ITEM_SWORD && id != ITEM_PICKAXE && id != ITEM_AXE && count > 1) {
                    int tens = count / 10; int ones = count % 10;
                    int num_y = by + 22; int num_x = bx + 19;
                    if (tens > 0) {
                        oam[i*2 + 32].attr0 = (num_y & 0x00FF) | 0x2000;
                        oam[i*2 + 32].attr1 = ((num_x - 4) & 0x01FF) | 0x0000;
                        oam[i*2 + 32].attr2 = (FONT_BASE_TILE * 2) + (tens * 2);
                    } else oam[i*2 + 32].attr0 = 0x0200;
                    oam[i*2 + 33].attr0 = (num_y & 0x00FF) | 0x2000;
                    oam[i*2 + 33].attr1 = (num_x & 0x01FF) | 0x0000;
                    oam[i*2 + 33].attr2 = (FONT_BASE_TILE * 2) + (ones * 2);
                } else { oam[i*2 + 32].attr0 = 0x0200; oam[i*2 + 33].attr0 = 0x0200; }
            }
            
            // Highlights
            int h_i = inv_cursor;
            int hb_x = (h_i < 12) ? (4 + (h_i % 4) * 28) : (124 + ((h_i - 12) % 4) * 28);
            int hb_y = (h_i < 12) ? (4 + (h_i / 4) * 28) : (4 + ((h_i - 12) / 4) * 28);
            oam[15].attr0 = (hb_y & 0x00FF) | 0x2000;
            oam[15].attr1 = (hb_x & 0x01FF) | 0x8000;
            oam[15].attr2 = 90;
            
            if (inv_held_cursor != -1) {
                int hh_i = inv_held_cursor;
                int hhb_x = (hh_i < 12) ? (4 + (hh_i % 4) * 28) : (124 + ((hh_i - 12) % 4) * 28);
                int hhb_y = (hh_i < 12) ? (4 + (hh_i / 4) * 28) : (4 + ((hh_i - 12) / 4) * 28);
                oam[16].attr0 = (hhb_y & 0x00FF) | 0x2000;
                oam[16].attr1 = (hhb_x & 0x01FF) | 0x8000;
                oam[16].attr2 = 90;
                if (frame_timer % 20 < 10) oam[16].attr0 |= 0x0200;
            } else oam[16].attr0 = 0x0200;
            
            if (!inv_open) {
                int ab_x = 4 + (active_slot % 4) * 28;
                int ab_y = 4 + (active_slot / 4) * 28;
                oam[15].attr0 = (ab_y & 0x00FF) | 0x2000;
                oam[15].attr1 = (ab_x & 0x01FF) | 0x8000;
                oam[15].attr2 = 90;
            }
        } else if (inv_open && inv_mode == 1) {
            // Crafting Interface (Available Recipes)
            // 1. CLEAR ALL UI OAMs (Numbers 32-79, Icons 80-103, Backgrounds 104-127)
            for (int i = 0; i < 24; i++) {
                oam[i + 80].attr0 = 0x0200; oam[i + 104].attr0 = 0x0200;
                oam[i*2 + 32].attr0 = 0x0200; oam[i*2 + 33].attr0 = 0x0200;
            }

            for(int r = 0; r < num_available; r++) {
                if (r >= 5) break; 
                int rx = 4; int ry = 4 + (r * 32);
                int r_idx = available_recipes[r];
                
                // Background
                oam[r + 104].attr0 = (ry & 0x00FF) | 0x2000;
                oam[r + 104].attr1 = (rx & 0x01FF) | 0x8000;
                oam[r + 104].attr2 = 58;
                
                // Result Item
                int res_base = recipes[r_idx].sprite_base;
                oam[r + 80].attr0 = ((ry + 8) & 0x00FF) | 0x2000;
                oam[r + 80].attr1 = ((rx + 8) & 0x01FF) | 0x4000; 
                oam[r + 80].attr2 = res_base;
                
                // Show Result Count (if > 1)
                int res_total = recipes[r_idx].res_qty;
                if (res_total > 1) {
                    int num_y = ry + 22; int num_x = rx + 19;
                    oam[r*2 + 32].attr0 = (num_y & 0x00FF) | 0x2000;
                    oam[r*2 + 32].attr1 = (num_x & 0x01FF) | 0x0000;
                    oam[r*2 + 32].attr2 = (FONT_BASE_TILE * 2) + (res_total * 2);
                }

                if (inv_cursor == r) {
                    oam[15].attr0 = (ry & 0x00FF) | 0x2000;
                    oam[15].attr1 = (rx & 0x01FF) | 0x8000;
                    oam[15].attr2 = 90;
                }
            }
        }
        
        // Hide unused slots when closed
        if (!inv_open) {
            inv_mode = 0; 
            inv_held_cursor = -1;
        }
        
        if (swing_timer > 0) {
            int cur_id = hotbar_id[active_slot];
            int item_base = 0;
            if (cur_id == ITEM_SWORD) item_base = 122;
            else if (cur_id == ITEM_PICKAXE) item_base = 138;
            else if (cur_id == ITEM_AXE) item_base = 154;
            else if (cur_id == ITEM_TORCH) item_base = 170;
            
            if (item_base > 0) {
                if (facing_left) item_base += 8; // Use Flipped frame (4 tiles * 2 indices)
                
                // Show in front of character (offset based on direction)
                int wx = draw_x + (facing_left ? -4 : 20);
                int wy = draw_y + 8;
                oam[31].attr0 = (wy & 0x00FF) | 0x2000;
                oam[31].attr1 = (wx & 0x01FF) | 0x4000; 
                oam[31].attr2 = item_base; 
            } else {
                oam[31].attr0 = 0x0200;
            }
        } else {
            oam[31].attr0 = 0x0200;
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
                fill_bg_map(BG_DIRT_WALL_TILE); 
            } else {
                fill_bg_map(BG_STONE_WALL_TILE); 
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
                    oam[i + 7].attr0 = (sy & 0x00FF) | 0x2000;
                    oam[i + 7].attr1 = (sx & 0x01FF) | 0x4000; // 16x16 size
                    int base_tile = (slimes[i].type == 1) ? 266 : 250;
                    oam[i + 7].attr2 = (slimes[i].dy < 0) ? (base_tile + 8) : base_tile; 
                } else oam[i + 7].attr0 = 0x0200;
            } else oam[i + 7].attr0 = 0x0200;
        }

        // Draw Sun (Loosely follow player)
        if (current_bg_type == 0) {
            int sun_sx = 180 - (cam_x / 16);
            int sun_sy = -10 - (cam_y / 16);
            oam[18].attr0 = (sun_sy & 0x00FF) | 0x2000;
            oam[18].attr1 = (sun_sx & 0x01FF) | 0x8000;
            oam[18].attr2 = (SUN_TILE * 2) | 0x0800; // Priority 2 (behind clouds)
        } else {
            oam[18].attr0 = 0x0200; // Hide
        }
        
        // Draw Smart Cursor
        if (smart_cx != -1 && smart_cy != -1) {
            int cx_draw = (smart_cx * 8) - cam_x;
            int cy_draw = (smart_cy * 8) - cam_y;
            if (cx_draw > -8 && cx_draw < 240 && cy_draw > -8 && cy_draw < 160) {
                oam[17].attr0 = (cy_draw & 0x00FF) | 0x2000;
                oam[17].attr1 = (cx_draw & 0x01FF) | 0x0000; // 8x8 size
                oam[17].attr2 = (SMART_CURSOR_TILE * 2); 
            } else {
                oam[17].attr0 = 0x0200;
            }
        } else {
            oam[17].attr0 = 0x0200;
        }

        // 5. Depth Meter Display
        if (has_depth) {
            int depth = (p_y >> 8) / 8 - 40; // 0 is surface approx
            int abs_depth = (depth < 0) ? -depth : depth;
            int dig1 = (abs_depth / 10) % 10;
            int dig2 = abs_depth % 10;
            
            // Draw Depth Text at bottom left
            oam[19].attr0 = (145 & 0x00FF) | 0x2000;
            oam[19].attr1 = (10 & 0x01FF) | 0x0000; // 8x8 size
            oam[19].attr2 = (FONT_BASE_TILE * 2) + (dig1 * 2);
            
            oam[20].attr0 = (145 & 0x00FF) | 0x2000;
            oam[20].attr1 = (18 & 0x01FF) | 0x0000;
            oam[20].attr2 = (FONT_BASE_TILE * 2) + (dig2 * 2);
        } else {
            oam[19].attr0 = 0x0200; oam[20].attr0 = 0x0200;
        }

        // Update tilemap (Only if moved)
        if (cam_x != prev_x || cam_y != prev_y) {
            update_map_seam(prev_x, prev_y, cam_x, cam_y);
            prev_x = cam_x;
            prev_y = cam_y;
        }

        // Dynamic Lighting (Punch holes in darkness using OBJ Window)
        int light_idx = 21; // Use indices 21-30 (10 sources total)
        if (current_darkness > 0) {
            // 1. Player's held torch light (Centered 64x64)
            int p_draw_x = (ix - cam_x) + 16 - 32; 
            int p_draw_y = (iy - cam_y - current_frame) + 16 - 32;
            if (hotbar_id[active_slot] == ITEM_TORCH) {
                oam[light_idx].attr0 = (p_draw_y & 0x00FF) | 0x2800; // OBJ Window mode
                oam[light_idx].attr1 = (p_draw_x & 0x01FF) | 0xC000; // 64x64 size
                oam[light_idx].attr2 = (MASK_BASE_TILE * 2);
                light_idx++;
            }
            // 2. Torches on the background (Scan nearby tiles)
            int start_tx = cam_x >> 3, start_ty = cam_y >> 3;
            for (int ty = start_ty - 3; ty < start_ty + 23; ty++) {
                if (ty < 0 || ty >= WORLD_H) continue;
                for (int tx = start_tx - 3; tx < start_tx + 33; tx++) {
                    if (tx >= 0 && tx < WORLD_W && world_map[ty][tx] == TILE_TORCH) {
                        int t_draw_x = (tx << 3) - cam_x - 28; // Center 64x64 on 8x8 torch
                        int t_draw_y = (ty << 3) - cam_y - 28;
                        oam[light_idx].attr0 = (t_draw_y & 0x00FF) | 0x2800; 
                        oam[light_idx].attr1 = (t_draw_x & 0x01FF) | 0xC000;
                        oam[light_idx].attr2 = (MASK_BASE_TILE * 2);
                        if (++light_idx >= 31) break;
                    }
                }
                if (light_idx >= 31) break;
            }
        }
        // Hide unused light OAMs
        for (; light_idx < 31; light_idx++) oam[light_idx].attr0 = 0x0200;

        // Optimized Tree Growth Tick (Target: Surface search only)
        for (int k = 0; k < 15; k++) {
            int rx = rand_next() % WORLD_W;
            int ry = rand_next() % 60; // Saplings only on surface
            if (world_map[ry][rx] == TILE_SAPLING) {
                grow_tree(rx, ry + 1, 1);
            }
        }

        // Respawn Logic
        if (p_health <= 0) {
            p_health = 100;
            p_x = spawn_x;
            p_y = spawn_y;
            p_dy = 0;
            p_flicker = 120;
            p_last_damage_timer = 600;
            
            // Instantly snap the camera to the spawn point
            cam_x = (p_x >> 8) - 120 + 16;
            cam_y = (p_y >> 8) - 80 + 16;
            if (cam_x < 0) cam_x = 0;
            if (cam_y < 0) cam_y = 0;
            if (cam_x > (WORLD_W * 8) - 240) cam_x = (WORLD_W * 8) - 240;
            if (cam_y > (WORLD_H * 8) - 160) cam_y = (WORLD_H * 8) - 160;
            
            // Force a full hardware map redraw on the next frame
            prev_x = -999;
            prev_y = -999;
            
            // Auto-save on death
            save_game();
        }
    }

    return 0;
}
