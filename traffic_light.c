/*
 * ARM Assembly Traffic Light Controller for Raspberry Pi 5
 * Two-way intersection with realistic timing
 * 
 * Hardware Setup:
 * Street A (North-South):
 *   - Red LED    -> GPIO 17 (Pin 11) + 330Ω resistor -> Ground
 *   - Yellow LED -> GPIO 27 (Pin 13) + 330Ω resistor -> Ground
 *   - Green LED  -> GPIO 22 (Pin 15) + 330Ω resistor -> Ground
 * 
 * Street B (East-West):
 *   - Red LED    -> GPIO 23 (Pin 16) + 330Ω resistor -> Ground
 *   - Yellow LED -> GPIO 24 (Pin 18) + 330Ω resistor -> Ground
 *   - Green LED  -> GPIO 25 (Pin 22) + 330Ω resistor -> Ground
 * 
 * All grounds connect to any Ground pin (e.g., Pin 6, 9, 14, 20, 25, 30, 34, 39)
 * 
 * Timing:
 * - Green: 5 seconds
 * - Yellow: 1 second
 * - Red: 5 seconds
 * - Safety buffer (both red): 1 second
 * 
 * Compile: gcc -o traffic_light traffic_light.c
 * Run: sudo ./traffic_light
 * 
 * Press Ctrl+C to exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

// BCM2712 (Raspberry Pi 5) GPIO registers
#define BCM2712_PERI_BASE   0x1f00000000
#define GPIO_BASE           (BCM2712_PERI_BASE + 0xd0000)
#define PAGE_SIZE           (4*1024)
#define BLOCK_SIZE          (4*1024)

// GPIO register offsets
#define GPFSEL0     0
#define GPFSEL1     1
#define GPFSEL2     2
#define GPSET0      7
#define GPCLR0      10

// GPIO Pin assignments
// Street A (North-South)
#define STREET_A_RED    17
#define STREET_A_YELLOW 27
#define STREET_A_GREEN  22

// Street B (East-West)
#define STREET_B_RED    23
#define STREET_B_YELLOW 24
#define STREET_B_GREEN  25

// Timing (in microseconds)
#define GREEN_TIME      5000000   // 5 seconds
#define YELLOW_TIME     1000000   // 1 second
#define RED_TIME        5000000   // 5 seconds (while other is green+yellow)
#define SAFETY_BUFFER   1000000   // 1 second both red

volatile uint32_t *gpio;
volatile int keep_running = 1;

// Signal handler for clean exit
void signal_handler(int sig) {
    keep_running = 0;
}

// Set GPIO pin as output using inline ARM assembly
void gpio_set_output(int pin) {
    uint32_t reg_offset = pin / 10;
    uint32_t bit_offset = (pin % 10) * 3;
    uint32_t value, mask, output_bits;
    
    // Load current register value
    __asm__ volatile (
        "ldr %w[value], [%[gpio_reg], %[offset], lsl #2]"
        : [value] "=r" (value)
        : [gpio_reg] "r" (gpio), [offset] "r" (reg_offset)
        : "memory"
    );
    
    // Clear the 3 bits for this pin
    mask = 0b111 << bit_offset;
    __asm__ volatile (
        "bic %w[value], %w[value], %w[mask]"
        : [value] "+r" (value)
        : [mask] "r" (mask)
    );
    
    // Set bit pattern for output (001)
    output_bits = 0b001 << bit_offset;
    __asm__ volatile (
        "orr %w[value], %w[value], %w[bits]"
        : [value] "+r" (value)
        : [bits] "r" (output_bits)
    );
    
    // Write back to register
    __asm__ volatile (
        "str %w[value], [%[gpio_reg], %[offset], lsl #2]"
        :
        : [value] "r" (value), [gpio_reg] "r" (gpio), [offset] "r" (reg_offset)
        : "memory"
    );
}

// Set GPIO pin HIGH using ARM assembly
void gpio_set_high(int pin) {
    uint32_t bit_mask = 1 << pin;
    
    __asm__ volatile (
        "str %w[mask], [%[gpio_reg], %[offset], lsl #2]"
        :
        : [mask] "r" (bit_mask), [gpio_reg] "r" (gpio), [offset] "r" (GPSET0)
        : "memory"
    );
}

// Set GPIO pin LOW using ARM assembly
void gpio_set_low(int pin) {
    uint32_t bit_mask = 1 << pin;
    
    __asm__ volatile (
        "str %w[mask], [%[gpio_reg], %[offset], lsl #2]"
        :
        : [mask] "r" (bit_mask), [gpio_reg] "r" (gpio), [offset] "r" (GPCLR0)
        : "memory"
    );
}

// Set multiple GPIO pins using ARM assembly (efficient batch operation)
void gpio_set_multiple(uint32_t pin_mask) {
    __asm__ volatile (
        "str %w[mask], [%[gpio_reg], %[offset], lsl #2]"
        :
        : [mask] "r" (pin_mask), [gpio_reg] "r" (gpio), [offset] "r" (GPSET0)
        : "memory"
    );
}

// Clear multiple GPIO pins using ARM assembly
void gpio_clear_multiple(uint32_t pin_mask) {
    __asm__ volatile (
        "str %w[mask], [%[gpio_reg], %[offset], lsl #2]"
        :
        : [mask] "r" (pin_mask), [gpio_reg] "r" (gpio), [offset] "r" (GPCLR0)
        : "memory"
    );
}

// Turn off all lights
void all_lights_off(void) {
    uint32_t all_pins = (1 << STREET_A_RED) | (1 << STREET_A_YELLOW) | (1 << STREET_A_GREEN) |
                        (1 << STREET_B_RED) | (1 << STREET_B_YELLOW) | (1 << STREET_B_GREEN);
    gpio_clear_multiple(all_pins);
}

// Set traffic light state
void set_light_state(int street_a_color, int street_b_color) {
    // Turn off all lights first
    all_lights_off();
    
    // Street A
    if (street_a_color == 'R') gpio_set_high(STREET_A_RED);
    else if (street_a_color == 'Y') gpio_set_high(STREET_A_YELLOW);
    else if (street_a_color == 'G') gpio_set_high(STREET_A_GREEN);
    
    // Street B
    if (street_b_color == 'R') gpio_set_high(STREET_B_RED);
    else if (street_b_color == 'Y') gpio_set_high(STREET_B_YELLOW);
    else if (street_b_color == 'G') gpio_set_high(STREET_B_GREEN);
}

// Print current state
void print_state(const char* street_a, const char* street_b, int cycle) {
    printf("\r[Cycle %03d] Street A (N-S): %-6s | Street B (E-W): %-6s", 
           cycle, street_a, street_b);
    fflush(stdout);
}

int main(void) {
    int mem_fd;
    void *gpio_map;
    
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║   ARM Assembly Traffic Light Controller - Pi 5        ║\n");
    printf("║   Two-Way Intersection Simulator                      ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    printf("Pin Configuration:\n");
    printf("  Street A (N-S): Red=GPIO%d, Yellow=GPIO%d, Green=GPIO%d\n", 
           STREET_A_RED, STREET_A_YELLOW, STREET_A_GREEN);
    printf("  Street B (E-W): Red=GPIO%d, Yellow=GPIO%d, Green=GPIO%d\n\n", 
           STREET_B_RED, STREET_B_YELLOW, STREET_B_GREEN);
    
    printf("Timing: Green=5s, Yellow=1s, Safety Buffer=1s\n\n");
    
    // Set up signal handler for Ctrl+C
    signal(SIGINT, signal_handler);
    
    // Open /dev/mem (requires root)
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0) {
        perror("Cannot open /dev/mem");
        printf("Try running with sudo!\n");
        return 1;
    }
    
    // Map GPIO memory
    gpio_map = mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, 
                    MAP_SHARED, mem_fd, GPIO_BASE);
    close(mem_fd);
    
    if (gpio_map == MAP_FAILED) {
        perror("mmap error");
        return 1;
    }
    
    gpio = (volatile uint32_t *)gpio_map;
    
    // Configure all GPIO pins as outputs
    printf("Configuring GPIO pins...\n");
    gpio_set_output(STREET_A_RED);
    gpio_set_output(STREET_A_YELLOW);
    gpio_set_output(STREET_A_GREEN);
    gpio_set_output(STREET_B_RED);
    gpio_set_output(STREET_B_YELLOW);
    gpio_set_output(STREET_B_GREEN);
    
    // Make sure all lights start off
    all_lights_off();
    
    printf("Starting traffic light sequence... (Press Ctrl+C to exit)\n\n");
    sleep(1);
    
    int cycle = 0;
    
    // Main traffic light loop
    while (keep_running) {
        cycle++;
        
        // Phase 1: Street A Green, Street B Red
        set_light_state('G', 'R');
        print_state("GREEN", "RED", cycle);
        usleep(GREEN_TIME);
        if (!keep_running) break;
        
        // Phase 2: Street A Yellow, Street B Red
        set_light_state('Y', 'R');
        print_state("YELLOW", "RED", cycle);
        usleep(YELLOW_TIME);
        if (!keep_running) break;
        
        // Phase 3: Both Red (Safety buffer)
        set_light_state('R', 'R');
        print_state("RED", "RED", cycle);
        usleep(SAFETY_BUFFER);
        if (!keep_running) break;
        
        // Phase 4: Street A Red, Street B Green
        set_light_state('R', 'G');
        print_state("RED", "GREEN", cycle);
        usleep(GREEN_TIME);
        if (!keep_running) break;
        
        // Phase 5: Street A Red, Street B Yellow
        set_light_state('R', 'Y');
        print_state("RED", "YELLOW", cycle);
        usleep(YELLOW_TIME);
        if (!keep_running) break;
        
        // Phase 6: Both Red (Safety buffer)
        set_light_state('R', 'R');
        print_state("RED", "RED", cycle);
        usleep(SAFETY_BUFFER);
        if (!keep_running) break;
    }
    
    // Clean up - turn off all lights
    printf("\n\nCleaning up...\n");
    all_lights_off();
    munmap(gpio_map, BLOCK_SIZE);
    
    printf("Traffic light stopped. Stay safe out there! 🚦\n");
    return 0;
}
