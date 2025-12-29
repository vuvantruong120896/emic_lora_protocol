/*=====================================================================
 * UART Printf Debug Test
 * 
 * This file helps verify printf() redirection to UART
 *=====================================================================*/

#include "hal_uart.h"
#include <stdio.h>

/**
 * test_uart_printf()
 * 
 * Test printf() output on UART
 * If you see output like "Test: 1" on terminal → printf works!
 */
void test_uart_printf(void)
{
    printf("\n");
    printf("=========================================\n");
    printf("  UART Printf Test\n");
    printf("=========================================\n");
    printf("\n");
    
    printf("1. Testing simple string:\n");
    printf("   Hello from printf()\n");
    printf("\n");
    
    printf("2. Testing integer output:\n");
    printf("   Value: %d\n", 42);
    printf("   Hex: 0x%02X\n", 0xA5);
    printf("\n");
    
    printf("3. Testing float output (if supported):\n");
    printf("   Pi: 3.14\n");
    printf("\n");
    
    printf("4. Testing multiple calls:\n");
    int i;
    for (i = 0; i < 3; i++)
    {
        printf("   Loop %d\n", i + 1);
    }
    printf("\n");
    
    printf("✓ Printf test complete!\n");
    printf("\n");
}

/**
 * test_uart_direct()
 * 
 * Test direct UART output (for comparison)
 * This should always work
 */
void test_uart_direct(void)
{
    printf("\n");
    printf("=========================================\n");
    printf("  Direct UART I/O Test\n");
    printf("=========================================\n");
    printf("\n");
    
    printf("1. Using hal_uart_puts():\n");
    hal_uart_puts("   Direct string output\n");
    printf("\n");
    
    printf("2. Using hal_uart_putchar():\n");
    hal_uart_putchar('A');
    hal_uart_putchar('B');
    hal_uart_putchar('C');
    hal_uart_putchar('\n');
    printf("\n");
    
    printf("✓ Direct I/O test complete!\n");
    printf("Test0: %d ms\n", 1234);
    printf("Test1: %ld ms\n", 65500);
    printf("Test2: %ld ms\n", 99999);
    printf("Test0: %x ms\n", 0x6589);
    printf("Test1: %x ms\n", 0xABCD);
    printf("Test2: %s\n", "Hello World");

    printf("\n");
}

/**
 * test_uart_combined()
 * 
 * Combined test - mix printf and direct output
 */
void test_uart_combined(void)
{
    printf("\n");
    printf("=========================================\n");
    printf("  Combined UART Test\n");
    printf("=========================================\n");
    printf("\n");
    
    printf("printf output: Using printf()\n");
    hal_uart_puts("Direct output: Using hal_uart_puts()\n");
    printf("printf output: Back to printf()\n");
    hal_uart_putchar('X');
    printf(" - after putchar\n");
    printf("\n");
    
    printf("✓ Combined test complete!\n");
    printf("\n");
}

/**
 * Compare output modes
 * 
 * Run this to debug printf issue:
 * 
 * If you see:
 *   ✓ hal_uart_send_buffer works (you see the custom message)
 *   ✗ printf() doesn't work (no printf output)
 * 
 * Then issue is with printf redirection
 * Run test_uart_printf() to verify
 */
void test_uart_all(void)
{
    test_uart_direct();     // Should definitely work
    test_uart_printf();     // Test printf redirection
    test_uart_combined();   // Test mixing methods
}
