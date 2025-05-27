#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h" 
#include "pico/multicore.h"
#include "GKM_LED.pio.h"

#define IS_RGBW false
#define NUM_PIXELS 25
#define GKM_LED_PIN 7

#define LED_RED_PIN 13
#define BTN_A_PIN 5
#define BTN_B_PIN 6

#define DEBOUNCE_DELAY_US 30000     
#define NUMBER_DISPLAY_DURATION_MS 1000 

volatile bool btn_a_pressed_flag = false;
volatile bool btn_b_pressed_flag = false;
bool sequence_a_active = false;
bool sequence_b_active = false;
int current_display_number_for_seq_a = 0;
uint64_t sequence_a_next_number_time_us = 0;
uint64_t sequence_b_end_time_us = 0;


bool numbers[10][NUM_PIXELS] = {
    { // Número 0
        0, 1, 1, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 1, 1, 0
    },
    { // Número 1
        0, 1, 1, 1, 0, 
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 1, 1, 1, 0
    },
    { // Número 2
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0, 
        0, 1, 1, 1, 0,
        0, 1, 0, 0, 0,
        0, 1, 1, 1, 0
    },
    { // Número 3
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0
    },
    { // Número 4
        0, 1, 0, 1, 0, 
        0, 1, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 0, 0, 1, 0
    },
    { // Número 5
        0, 1, 1, 1, 0,
        0, 1, 0, 0, 0,
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0
    },
    { // Número 6 
        0, 1, 1, 1, 0,
        0, 1, 0, 0, 0, 
        0, 1, 1, 1, 0, 
        0, 1, 0, 1, 0, 
        0, 1, 1, 1, 0  
    }
};

bool all_off_pattern[NUM_PIXELS] = {false}; 

bool face_pattern[NUM_PIXELS] = {
    0, 1, 0, 1, 0,  
    0, 0, 0, 0, 0,  
    1, 0, 0, 0, 1,  
    0, 1, 1, 1, 0,  
    0, 0, 0, 0, 0   
};


static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}


void display_pattern(bool *buffer, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = urgb_u32(r, g, b);
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(buffer[i] ? color : 0); 
    }
}

void setup() {
    
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);

    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);

    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
}

void button_callback(uint gpio, uint32_t events) {
    static volatile uint64_t last_interrupt_time_a = 0;
    static volatile uint64_t last_interrupt_time_b = 0;
    uint64_t current_time = time_us_64();

    if (sequence_a_active || sequence_b_active) {
        return;
    }

    if (gpio == BTN_A_PIN) {
        if (current_time - last_interrupt_time_a > DEBOUNCE_DELAY_US) {
            btn_a_pressed_flag = true;
            last_interrupt_time_a = current_time;
        }
    } else if (gpio == BTN_B_PIN) {
        if (current_time - last_interrupt_time_b > DEBOUNCE_DELAY_US) {
            btn_b_pressed_flag = true;
            last_interrupt_time_b = current_time;
        }
    }
}

void led_red_blink() {
    while (1) {
        gpio_put(LED_RED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_RED_PIN, 0);
        sleep_ms(100);
    }
}

int main() {
    stdio_init_all();
    setup();

    multicore_launch_core1(led_red_blink);

    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &GKM_LED_program);
    GKM_LED_program_init(pio, sm, offset, GKM_LED_PIN, 800000, IS_RGBW);

    gpio_set_irq_enabled_with_callback(BTN_A_PIN, GPIO_IRQ_EDGE_FALL, true, &button_callback);
    gpio_set_irq_enabled_with_callback(BTN_B_PIN, GPIO_IRQ_EDGE_FALL, true, &button_callback);

    display_pattern(all_off_pattern, 0, 0, 0);

    while (1) {
        uint64_t now_us = time_us_64();

        if (btn_a_pressed_flag) {
            btn_a_pressed_flag = false; 
            if (!sequence_a_active && !sequence_b_active) { 
                sequence_a_active = true;
                current_display_number_for_seq_a = 0;
                display_pattern(numbers[current_display_number_for_seq_a], 0, 0, 200); 
                sequence_a_next_number_time_us = now_us + (uint64_t)NUMBER_DISPLAY_DURATION_MS * 1000;
            }
        }

        if (btn_b_pressed_flag) {
            btn_b_pressed_flag = false; 
            if (!sequence_a_active && !sequence_b_active) { 
                sequence_b_active = true;
                display_pattern(face_pattern, 60, 30, 0); 
                sequence_b_end_time_us = now_us + 5000000; 
            }
        }

        if (sequence_a_active) {
            if (current_display_number_for_seq_a <= 5) { 
                if (now_us >= sequence_a_next_number_time_us) {
                    current_display_number_for_seq_a++;
                    if (current_display_number_for_seq_a <= 5) {
                        display_pattern(numbers[current_display_number_for_seq_a], 0, 0, 200); 
                        sequence_a_next_number_time_us = now_us + (uint64_t)NUMBER_DISPLAY_DURATION_MS * 1000;
                    } else {
                        display_pattern(all_off_pattern, 0, 0, 0);
                        sequence_a_active = false;
                    }
                }
            } else {
                display_pattern(all_off_pattern, 0, 0, 0);
                sequence_a_active = false;
            }
        }
        else if (sequence_b_active) {
            if (now_us >= sequence_b_end_time_us) {
                display_pattern(all_off_pattern, 0, 0, 0); 
                sequence_b_active = false;
            }
        }
        else {
            sleep_ms(10);
        }
    }
    return 0; 
}