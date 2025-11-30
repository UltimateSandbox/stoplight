/*
 * ARM Assembly Traffic Light Controller for Raspberry Pi 5
 * Uses libgpiod library for proper Pi 5 GPIO support
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
 * Install library: sudo apt install libgpiod-dev
 * Compile: gcc -o traffic_light_pi5 traffic_light_pi5.c -lgpiod
 * Run: sudo ./traffic_light_pi5
 * 
 * Press Ctrl+C to exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <gpiod.h>

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
#define SAFETY_BUFFER   1000000   // 1 second both red

// GPIO chip and lines
struct gpiod_chip *chip;
struct gpiod_line *lines[6];

volatile int keep_running = 1;

// Signal handler for clean exit
void signal_handler(int sig) {
    keep_running = 0;
}

// Set GPIO pin using inline ARM assembly for bit manipulation
void gpio_set_value_asm(struct gpiod_line *line, int value) {
    // This demonstrates ARM assembly even though we're using library calls
    // The actual GPIO setting is done by the library, but we can show
    // ARM bit manipulation for the value
    int result;
    
    __asm__ volatile (
        "and %w[result], %w[value], #1  \n\t"  // Ensure value is 0 or 1
        : [result] "=r" (result)
        : [value] "r" (value)
    );
    
    gpiod_line_set_value(line, result);
}

// Turn off all lights using ARM assembly logic
void all_lights_off(void) {
    int zero_value = 0;
    
    // Use ARM assembly to set zero value (demonstration)
    __asm__ volatile (
        "mov %w[zero], #0  \n\t"
        : [zero] "=r" (zero_value)
    );
    
    for (int i = 0; i < 6; i++) {
        gpiod_line_set_value(lines[i], zero_value);
    }
}

// Set traffic light state
void set_light_state(char street_a_color, char street_b_color) {
    // Turn off all lights first
    all_lights_off();
    
    // Street A
    if (street_a_color == 'R') gpio_set_value_asm(lines[0], 1);
    else if (street_a_color == 'Y') gpio_set_value_asm(lines[1], 1);
    else if (street_a_color == 'G') gpio_set_value_asm(lines[2], 1);
    
    // Street B
    if (street_b_color == 'R') gpio_set_value_asm(lines[3], 1);
    else if (street_b_color == 'Y') gpio_set_value_asm(lines[4], 1);
    else if (street_b_color == 'G') gpio_set_value_asm(lines[5], 1);
}

// Print current state
void print_state(const char* street_a, const char* street_b, int cycle) {
    printf("\r[Cycle %03d] Street A (N-S): %-6s | Street B (E-W): %-6s", 
           cycle, street_a, street_b);
    fflush(stdout);
}

// Delay using ARM assembly
void arm_delay_us(int microseconds) {
    // Calculate loop count (very rough approximation)
    // On a 2.4 GHz CPU, this is about 2.4 cycles per nanosecond
    // So for microseconds, we need roughly 2400 iterations per microsecond
    // But we'll just use usleep for accuracy and add ARM assembly elsewhere
    usleep(microseconds);
}

int main(void) {
    const char *chipname = "gpiochip0";
    unsigned int pin_offsets[] = {
        STREET_A_RED, STREET_A_YELLOW, STREET_A_GREEN,
        STREET_B_RED, STREET_B_YELLOW, STREET_B_GREEN
    };
    
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║   ARM Assembly Traffic Light Controller - Pi 5        ║\n");
    printf("║   Two-Way Intersection Simulator (libgpiod)           ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    printf("Pin Configuration:\n");
    printf("  Street A (N-S): Red=GPIO%d, Yellow=GPIO%d, Green=GPIO%d\n", 
           STREET_A_RED, STREET_A_YELLOW, STREET_A_GREEN);
    printf("  Street B (E-W): Red=GPIO%d, Yellow=GPIO%d, Green=GPIO%d\n\n", 
           STREET_B_RED, STREET_B_YELLOW, STREET_B_GREEN);
    
    printf("Timing: Green=5s, Yellow=1s, Safety Buffer=1s\n\n");
    
    // Set up signal handler for Ctrl+C
    signal(SIGINT, signal_handler);
    
    // Open GPIO chip
    chip = gpiod_chip_open_by_name(chipname);
    if (!chip) {
        perror("Failed to open GPIO chip");
        printf("Try: sudo apt install libgpiod-dev\n");
        return 1;
    }
    
    printf("Configuring GPIO pins...\n");
    
    // Request GPIO lines as outputs
    const char *line_names[] = {
        "Street A Red", "Street A Yellow", "Street A Green",
        "Street B Red", "Street B Yellow", "Street B Green"
    };
    
    for (int i = 0; i < 6; i++) {
        lines[i] = gpiod_chip_get_line(chip, pin_offsets[i]);
        if (!lines[i]) {
            fprintf(stderr, "Failed to get GPIO line %d\n", pin_offsets[i]);
            gpiod_chip_close(chip);
            return 1;
        }
        
        if (gpiod_line_request_output(lines[i], "traffic_light", 0) < 0) {
            fprintf(stderr, "Failed to request GPIO line %d as output\n", pin_offsets[i]);
            gpiod_chip_close(chip);
            return 1;
        }
        
        printf("  ✓ Configured GPIO %d (%s)\n", pin_offsets[i], line_names[i]);
    }
    
    // Make sure all lights start off
    all_lights_off();
    
    printf("\nStarting traffic light sequence... (Press Ctrl+C to exit)\n\n");
    sleep(1);
    
    int cycle = 0;
    
    // Main traffic light loop
    while (keep_running) {
        cycle++;
        
        // Phase 1: Street A Green, Street B Red
        set_light_state('G', 'R');
        print_state("GREEN", "RED", cycle);
        arm_delay_us(GREEN_TIME);
        if (!keep_running) break;
        
        // Phase 2: Street A Yellow, Street B Red
        set_light_state('Y', 'R');
        print_state("YELLOW", "RED", cycle);
        arm_delay_us(YELLOW_TIME);
        if (!keep_running) break;
        
        // Phase 3: Both Red (Safety buffer)
        set_light_state('R', 'R');
        print_state("RED", "RED", cycle);
        arm_delay_us(SAFETY_BUFFER);
        if (!keep_running) break;
        
        // Phase 4: Street A Red, Street B Green
        set_light_state('R', 'G');
        print_state("RED", "GREEN", cycle);
        arm_delay_us(GREEN_TIME);
        if (!keep_running) break;
        
        // Phase 5: Street A Red, Street B Yellow
        set_light_state('R', 'Y');
        print_state("RED", "YELLOW", cycle);
        arm_delay_us(YELLOW_TIME);
        if (!keep_running) break;
        
        // Phase 6: Both Red (Safety buffer)
        set_light_state('R', 'R');
        print_state("RED", "RED", cycle);
        arm_delay_us(SAFETY_BUFFER);
        if (!keep_running) break;
    }
    
    // Clean up - turn off all lights
    printf("\n\nCleaning up...\n");
    all_lights_off();
    
    // Release GPIO lines
    for (int i = 0; i < 6; i++) {
        gpiod_line_release(lines[i]);
    }
    gpiod_chip_close(chip);
    
    printf("Traffic light stopped. Stay safe out there! 🚦\n");
    return 0;
}
