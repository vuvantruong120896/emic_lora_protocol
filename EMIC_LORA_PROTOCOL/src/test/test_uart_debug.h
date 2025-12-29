/*=====================================================================
 * UART Printf Debug Test - Header
 *=====================================================================*/

#ifndef TEST_UART_DEBUG_H
#define TEST_UART_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * test_uart_printf()
 * Test printf() redirection to UART
 */
void test_uart_printf(void);

/**
 * test_uart_direct()
 * Test direct UART functions (hal_uart_puts, hal_uart_putchar)
 */
void test_uart_direct(void);

/**
 * test_uart_combined()
 * Test mixing printf and direct UART calls
 */
void test_uart_combined(void);

/**
 * test_uart_all()
 * Run all UART tests
 */
void test_uart_all(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_UART_DEBUG_H */
