#include <stdbool.h>

// Memory-mapped I/O addresses
volatile short *pixel_buffer = (volatile short *)0xC8000000;
volatile int   *PS2_ptr      = (volatile int *)0xFF200100;
volatile int   *AUDIO_ptr    = (volatile int *)0xFF203040; 
volatile int   *HEX3_HEX0_ptr= (volatile int *)0xFF200020; 
volatile int   *HEX5_HEX4_ptr= (volatile int *)0xFF200030; 

// Screen + colors (RGB565)
#define SW 320
#define SH 240

#define C_BLACK  0x0000
#define C_WHITE  0xFFFF
#define C_GREY   0x8410
#define C_RED    0xF800 
#define C_GREEN  0x07E0 
#define C_YELLOW 0xFFE0 
#define C_BLUE   0x001F 
#define C_SKIN   0xFEA0
#define C_LBLUE  0x07FF
#define C_ORANGE 0xFD20 
#define C_SHADOW 0x4208 

// Quick random helper
unsigned int seed = 9876;
unsigned int fast_rand() {
    seed = (1103515245 * seed + 12345);
    return (unsigned int)(seed / 65536) % 32768;
}

// Simple audio + HEX display helpers
void play_sfx(int frequency, int duration_ms) {
    int total_samples = (8000 * duration_ms) / 1000;
    int half_period = 8000 / (frequency * 2);
    if (half_period == 0) half_period = 1;

    for (int i = 0; i < total_samples; i++) {
        int fifospace = *(AUDIO_ptr + 1);
        int wslc = (fifospace & 0x00FF0000) >> 16; 
        while (wslc == 0) {
            fifospace = *(AUDIO_ptr + 1);
            wslc = (fifospace & 0x00FF0000) >> 16;
        }
        int sample = ((i / half_period) % 2) ? 0x01FFFFFF : -0x01FFFFFF;
        *(AUDIO_ptr + 2) = sample; 
        *(AUDIO_ptr + 3) = sample; 
    }
}

void update_high_score_leds(int score) {
    int hex_codes[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    
    int d0 = score % 10;
    int d1 = (score / 10) % 10;
    int d2 = (score / 100) % 10;
    int d3 = (score / 1000) % 10;
    int d4 = (score / 10000) % 10;
    int d5 = (score / 100000) % 10;

    *HEX3_HEX0_ptr = (hex_codes[d3] << 24) | (hex_codes[d2] << 16) | (hex_codes[d1] << 8) | hex_codes[d0];
    *HEX5_HEX4_ptr = (hex_codes[d5] << 8) | hex_codes[d4];
}

// Drawing basics
void draw_pixel(int x, int y, short color) {
    if (x >= 0 && x < SW && y >= 0 && y < SH) {
        pixel_buffer[(y << 9) + x] = color;
    }
}
void draw_rect(int x, int y, int width, int height, short color) {
    for (int i = x; i < x + width; i++) {
        for (int j = y; j < y + height; j++) draw_pixel(i, j, color);
    }
}

// Tiny play/pause icons
void draw_play_icon(int x, int y, short color) {
    for (int i = 0; i < 20; i++) {
        draw_rect(x + i*2, y + i*2, 2, 80 - i*4, color);
    }
}
void draw_pause_icon(int x, int y, short color) {
    draw_rect(x, y, 15, 50, color);
    draw_rect(x + 25, y, 15, 50, color);
}

// Mini 3x5 digit font
void draw_digit(int digit, int x, int y, short color) {
    int font[10][5] = {
        {0x7,0x5,0x5,0x5,0x7}, {0x2,0x6,0x2,0x2,0x7}, {0x7,0x1,0x7,0x4,0x7}, 
        {0x7,0x1,0x7,0x1,0x7}, {0x5,0x5,0x7,0x1,0x1}, {0x7,0x4,0x7,0x1,0x7}, 
        {0x7,0x4,0x7,0x5,0x7}, {0x7,0x1,0x1,0x1,0x1}, {0x7,0x5,0x7,0x5,0x7}, 
        {0x7,0x5,0x7,0x1,0x7}
    };
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 3; c++) {
            if ((font[digit][r] >> (2 - c)) & 1) draw_rect(x + c*2, y + r*2, 2, 2, color); 
        }
    }
}
void draw_number(int num, int x, int y, short color) {
    if (num == 0) { draw_digit(0, x, y, color); return; }
    int digits[10], count = 0;
    while (num > 0) { digits[count++] = num % 10; num /= 10; }
    for (int i = 0; i < count; i++) draw_digit(digits[count - 1 - i], x + i * 8, y, color); 
}

// Pixel art bits
void draw_boy(int x, int y, bool is_jumping) {
    int yo = is_jumping ? -15 : 0; 
    if (is_jumping) draw_rect(x+4, y+28, 12, 4, C_SHADOW); 
    draw_rect(x+6, y+yo, 8, 8, C_SKIN);      
    draw_rect(x+2, y+8+yo, 16, 14, C_BLUE);  
    draw_rect(x-2, y+8+yo, 4, 10, C_SKIN);   
    draw_rect(x+18, y+8+yo, 4, 10, C_SKIN);  
    draw_rect(x+4, y+22+yo, 4, 10, C_BLACK); 
    draw_rect(x+12, y+22+yo, 4, 10, C_BLACK);
}
void draw_train(int x, int y, short color) {
    draw_rect(x, y, 30, 50, color);       
    draw_rect(x+5, y+5, 20, 15, C_LBLUE); 
    draw_rect(x+5, y+40, 6, 6, C_YELLOW); 
    draw_rect(x+19, y+40, 6, 6, C_YELLOW);
    draw_rect(x+10, y+25, 10, 10, C_GREY);
}
void draw_hurdle(int x, int y) {
    draw_rect(x, y+10, 30, 5, C_ORANGE);  
    draw_rect(x+4, y, 4, 15, C_WHITE);    
    draw_rect(x+22, y, 4, 15, C_WHITE);   
}
void draw_coin(int x, int y) {
    draw_rect(x+4, y, 8, 16, C_YELLOW);
    draw_rect(x, y+4, 16, 8, C_YELLOW);
    draw_rect(x+4, y+4, 8, 8, C_BLACK);   
}
void draw_cherry(int x, int y) {
    draw_rect(x+8, y, 2, 8, C_BLACK);     
    draw_rect(x+8, y, 8, 2, C_BLACK);     
    draw_rect(x+14, y, 2, 8, C_BLACK);    
    draw_rect(x, y+6, 8, 8, C_RED);       
    draw_rect(x+12, y+6, 8, 8, C_RED);    
}

void delay(int count) { volatile int i; for (i = 0; i < count; i++); }

// Game data
typedef struct { int y; int lane; short color; } Train;
typedef struct { int y; int lane; } Entity;

#define NUM_TRAINS 3
#define NUM_COINS 4
#define NUM_HURDLES 2

// Global state
int lanes[3] = {113, 160, 207}; 
short t_colors[4] = {C_RED, C_BLUE, C_YELLOW, C_LBLUE};

Train trains[NUM_TRAINS];
Entity coins[NUM_COINS];
Entity hurdles[NUM_HURDLES];
Entity cherry;

int p_lane, p_y = 190, p_w = 20, p_h = 32;
bool is_jumping;
int jump_timer, global_speed, score, coin_count, cherry_count, high_score = 0;

void reset_game() {
    p_lane = 1;       
    is_jumping = false;
    jump_timer = 0;
    score = 0;
    coin_count = 0;
    cherry_count = 0;
    global_speed = 6;
    cherry.y = -800; cherry.lane = 1;
    
    for(int i=0; i<NUM_TRAINS; i++)  { trains[i].y = -100 - (i*150); trains[i].lane = i%3; trains[i].color = t_colors[i%4]; }
    for(int i=0; i<NUM_COINS; i++)   { coins[i].y = -50 - (i*80); coins[i].lane = fast_rand()%3; }
    for(int i=0; i<NUM_HURDLES; i++) { hurdles[i].y = -200 - (i*200); hurdles[i].lane = fast_rand()%3; }
    
    draw_rect(0, 0, SW, SH, C_GREEN);     
    draw_rect(90, 0, 140, SH, C_GREY);    
    draw_rect(136, 0, 2, SH, C_WHITE);    
    draw_rect(182, 0, 2, SH, C_WHITE);    
}

// State machine
typedef enum { MENU, PLAYING, PAUSED, GAME_OVER } GameState;

int main(void) {
    GameState state = MENU;
    bool ignore_next = false; 
    
    update_high_score_leds(0);

    // Draw the menu background once
    draw_rect(0, 0, SW, SH, C_GREEN);
    draw_rect(90, 0, 140, SH, C_GREY);
    draw_rect(136, 0, 2, SH, C_WHITE);
    draw_rect(182, 0, 2, SH, C_WHITE);

    while (1) {
        // Read PS/2 input
        bool btn_left = false, btn_right = false, btn_jump = false, btn_start = false;
        int ps2_data = *PS2_ptr;
        if (ps2_data & 0x8000) { 
            int byte = ps2_data & 0xFF; 
            if (byte == 0xF0) ignore_next = true;
            else if (ignore_next) ignore_next = false;
            else {
                if (byte == 0x6B) btn_left = true;
                if (byte == 0x74) btn_right = true;
                if (byte == 0x75) btn_jump = true; 
                if (byte == 0x29) btn_start = true; // Spacebar
            }
        }

        // Menu
        if (state == MENU) {
            draw_rect(110, 70, 100, 100, C_BLACK);
            draw_rect(112, 72, 96, 96, C_BLUE);
            draw_play_icon(140, 80, C_WHITE);
            
            if (btn_start || btn_jump) {
                reset_game();
                state = PLAYING;
                play_sfx(2000, 200); 
            }
            delay(100000);
        }
        
        // Pause
        else if (state == PAUSED) {
            draw_rect(110, 70, 100, 100, C_BLACK);
            draw_rect(112, 72, 96, 96, C_BLUE);
            draw_pause_icon(140, 95, C_WHITE); // Draws the parallel lines
            
            if (btn_start) {
                state = PLAYING;
                play_sfx(2000, 200);
                
                // Erase the pause menu box to prevent ghosting
                draw_rect(110, 70, 100, 100, C_GREY);
                draw_rect(136, 70, 2, 100, C_WHITE);
                draw_rect(182, 70, 2, 100, C_WHITE);
            }
            delay(100000);
        }
        
        // Gameplay
        else if (state == PLAYING) {
            
            if (btn_start) { // Spacebar pauses mid-run
                state = PAUSED;
                play_sfx(1000, 100);
                continue; // Skip drawing this frame to instantly show pause menu
            }
            
            // Player movement + simple ghosting fix
            int old_p_lane = p_lane; // keep old lane to erase cleanly
            
            if (btn_left && p_lane > 0) p_lane--; 
            if (btn_right && p_lane < 2) p_lane++; 
            if (btn_jump && !is_jumping) { is_jumping = true; jump_timer = 15; }
            if (is_jumping) { jump_timer--; if (jump_timer <= 0) is_jumping = false; }

            // erase the old boy position
            draw_rect(lanes[old_p_lane] - 15, p_y - 15, 30, 50, C_GREY); 
            
            for(int i=0; i<NUM_TRAINS; i++) draw_rect(lanes[trains[i].lane]-15, trains[i].y, 30, 50, C_GREY);
            for(int i=0; i<NUM_COINS; i++) draw_rect(lanes[coins[i].lane]-10, coins[i].y, 20, 20, C_GREY);
            for(int i=0; i<NUM_HURDLES; i++) draw_rect(lanes[hurdles[i].lane]-15, hurdles[i].y, 30, 15, C_GREY);
            draw_rect(lanes[cherry.lane]-10, cherry.y, 25, 20, C_GREY);
            
            draw_rect(136, 0, 2, SH, C_WHITE);
            draw_rect(182, 0, 2, SH, C_WHITE);

            // Move everything + respawn when off-screen
            for(int i=0; i<NUM_TRAINS; i++) {
                trains[i].y += global_speed;
                if (trains[i].y > SH) { trains[i].y = -100-(fast_rand()%300); trains[i].lane = fast_rand()%3; trains[i].color = t_colors[fast_rand()%4]; }
            }
            for(int i=0; i<NUM_COINS; i++) {
                coins[i].y += global_speed;
                if (coins[i].y > SH) { coins[i].y = -50-(fast_rand()%200); coins[i].lane = fast_rand()%3; }
            }
            for(int i=0; i<NUM_HURDLES; i++) {
                hurdles[i].y += global_speed;
                if (hurdles[i].y > SH) { hurdles[i].y = -200-(fast_rand()%400); hurdles[i].lane = fast_rand()%3; }
            }
            cherry.y += global_speed;
            if (cherry.y > SH) { cherry.y = -800-(fast_rand()%600); cherry.lane = fast_rand()%3; }

            // Collisions
            int px = lanes[p_lane] - (p_w/2);
            for(int i=0; i<NUM_TRAINS; i++) {
                if (p_lane == trains[i].lane && p_y < trains[i].y+50 && p_y+p_h > trains[i].y) {
                    state = GAME_OVER; play_sfx(150, 600);
                }
            }
            for(int i=0; i<NUM_HURDLES; i++) {
                if (p_lane == hurdles[i].lane && p_y < hurdles[i].y+15 && p_y+p_h > hurdles[i].y) {
                    if (!is_jumping) { state = GAME_OVER; play_sfx(150, 600); }
                }
            }
            for(int i=0; i<NUM_COINS; i++) {
                if (p_lane == coins[i].lane && p_y < coins[i].y+16 && p_y+p_h > coins[i].y) {
                    score += 10; coin_count++; coins[i].y = SH + 1; play_sfx(1200, 50);
                }
            }
            if (p_lane == cherry.lane && p_y < cherry.y+14 && p_y+p_h > cherry.y) {
                score += 50; cherry_count++; cherry.y = SH + 1; play_sfx(1800, 100);
            }

            // Speed ramps up with score
            if (score > 0 && score % 150 == 0) global_speed = 7; 
            if (score > 400) global_speed = 8;
            if (score > 800) global_speed = 9;

            // Draw everything + HUD
            for(int i=0; i<NUM_COINS; i++) draw_coin(lanes[coins[i].lane]-8, coins[i].y);
            for(int i=0; i<NUM_HURDLES; i++) draw_hurdle(lanes[hurdles[i].lane]-15, hurdles[i].y);
            for(int i=0; i<NUM_TRAINS; i++) draw_train(lanes[trains[i].lane]-15, trains[i].y, trains[i].color);
            draw_cherry(lanes[cherry.lane]-10, cherry.y);
            draw_boy(px, p_y, is_jumping);
            
            draw_rect(240, 5, 70, 40, C_GREEN); 
            draw_coin(240, 10);   draw_number(coin_count, 265, 14, C_WHITE);
            draw_cherry(240, 30); draw_number(cherry_count, 265, 34, C_WHITE);

            delay(250000); 
        }
        
        // Game over
        else if (state == GAME_OVER) {
            int px = lanes[p_lane] - (p_w/2);
            draw_rect(px, p_y, p_w, p_h, C_RED); 
            
            if (score > high_score) {
                high_score = score;
                update_high_score_leds(high_score);
            }

            // Draw play icon again as a replay prompt
            draw_rect(110, 70, 100, 100, C_BLACK);
            draw_rect(112, 72, 96, 96, C_RED);
            draw_play_icon(140, 80, C_WHITE);
            
            if (btn_start || btn_jump) {
                reset_game();
                state = PLAYING;
                play_sfx(2000, 200);
            }
            delay(100000);
        }
    }
    return 0;
}
