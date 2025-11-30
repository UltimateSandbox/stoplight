/*
 * GPIO Diagnostic Test for Raspberry Pi 5
 * Tests GPIO access and reports what's happening
 * 
 * Compile: gcc -o gpio_test gpio_test.c
 * Run: sudo ./gpio_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

// Try different possible base addresses for Pi 5
#define BCM2712_PERI_BASE_1   0x1f00000000ULL
#define BCM2712_PERI_BASE_2   0xfe000000ULL
#define GPIO_OFFSET           0xd0000

#define GPFSEL1     1
#define GPSET0      7
#define GPCLR0      10

#define TEST_PIN    17  // GPIO 17

int test_gpio_address(uint64_t base_addr) {
    int mem_fd;
    void *gpio_map;
    volatile uint32_t *gpio;
    uint64_t gpio_base = base_addr + GPIO_OFFSET;
    
    printf("\nTesting GPIO base address: 0x%llx\n", gpio_base);
    
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0) {
        printf("  ✗ Cannot open /dev/mem\n");
        return 0;
    }
    
    gpio_map = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, gpio_base);
    close(mem_fd);
    
    if (gpio_map == MAP_FAILED) {
        printf("  ✗ mmap failed\n");
        return 0;
    }
    
    gpio = (volatile uint32_t *)gpio_map;
    
    // Try to read GPFSEL1
    uint32_t gpfsel1_value = gpio[GPFSEL1];
    printf("  ✓ mmap succeeded\n");
    printf("  GPFSEL1 value: 0x%08x\n", gpfsel1_value);
    
    // Try to configure GPIO 17 as output
    uint32_t value = gpio[GPFSEL1];
    value &= ~(0b111 << 21);  // Clear bits for GPIO 17
    value |= (0b001 << 21);   // Set as output
    gpio[GPFSEL1] = value;
    
    printf("  Configured GPIO 17 as output\n");
    
    // Try to toggle the pin
    printf("  Testing GPIO 17 toggle...\n");
    
    for (int i = 0; i < 5; i++) {
        gpio[GPSET0] = (1 << TEST_PIN);  // Set high
        printf("    Set HIGH\n");
        usleep(500000);
        
        gpio[GPCLR0] = (1 << TEST_PIN);  // Set low
        printf("    Set LOW\n");
        usleep(500000);
    }
    
    munmap(gpio_map, 4096);
    return 1;
}

int test_gpiomem() {
    int gpio_fd;
    void *gpio_map;
    volatile uint32_t *gpio;
    
    printf("\nTesting /dev/gpiomem access...\n");
    
    if ((gpio_fd = open("/dev/gpiomem", O_RDWR|O_SYNC)) < 0) {
        printf("  ✗ Cannot open /dev/gpiomem\n");
        return 0;
    }
    
    gpio_map = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, gpio_fd, 0);
    close(gpio_fd);
    
    if (gpio_map == MAP_FAILED) {
        printf("  ✗ mmap failed\n");
        return 0;
    }
    
    gpio = (volatile uint32_t *)gpio_map;
    
    uint32_t gpfsel1_value = gpio[GPFSEL1];
    printf("  ✓ /dev/gpiomem access succeeded\n");
    printf("  GPFSEL1 value: 0x%08x\n", gpfsel1_value);
    
    // Configure GPIO 17 as output
    uint32_t value = gpio[GPFSEL1];
    value &= ~(0b111 << 21);
    value |= (0b001 << 21);
    gpio[GPFSEL1] = value;
    
    printf("  Configured GPIO 17 as output\n");
    printf("  Testing GPIO 17 toggle...\n");
    
    for (int i = 0; i < 5; i++) {
        gpio[GPSET0] = (1 << TEST_PIN);
        printf("    Set HIGH (LED should be ON)\n");
        usleep(500000);
        
        gpio[GPCLR0] = (1 << TEST_PIN);
        printf("    Set LOW (LED should be OFF)\n");
        usleep(500000);
    }
    
    munmap(gpio_map, 4096);
    return 1;
}

int main(void) {
    printf("╔══════════════════════════════════════════╗\n");
    printf("║  Raspberry Pi 5 GPIO Diagnostic Tool    ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    
    printf("\nThis will test GPIO 17 (Pin 11)\n");
    printf("Watch for your LED to blink!\n");
    
    // First try /dev/gpiomem (preferred method)
    if (test_gpiomem()) {
        printf("\n✓ SUCCESS with /dev/gpiomem!\n");
        printf("Your code should use /dev/gpiomem instead of /dev/mem\n");
        return 0;
    }
    
    // Try different base addresses
    if (test_gpio_address(BCM2712_PERI_BASE_1)) {
        printf("\n✓ SUCCESS with base 0x%llx!\n", BCM2712_PERI_BASE_1);
        return 0;
    }
    
    if (test_gpio_address(BCM2712_PERI_BASE_2)) {
        printf("\n✓ SUCCESS with base 0x%llx!\n", BCM2712_PERI_BASE_2);
        return 0;
    }
    
    printf("\n✗ All methods failed. This might be a permissions or kernel issue.\n");
    printf("\nDebug info:\n");
    printf("- Make sure you're running with sudo\n");
    printf("- Check that GPIO is enabled in raspi-config\n");
    printf("- Verify you're on Raspberry Pi OS (not Ubuntu)\n");
    
    return 1;
}
