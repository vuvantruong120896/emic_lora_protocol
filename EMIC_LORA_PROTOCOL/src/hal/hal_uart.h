/*=====================================================================
 * HAL UART - UART Abstraction Layer
 * 
 * Description:
 *   Hardware abstraction layer for UART communication
 *   Uses Config_UARTA1 from Smart Configurator
 *   
 * MCU: R7F100GGGxFB (RL78 G23)
 * UART: UART A1 (P3.0 TX, P3.1 RX)
 * Baud Rate: 9600
 * 
 * Date: December 2025
 *=====================================================================*/

#ifndef HAL_UART_H
#define HAL_UART_H

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
*                                        INCLUDE FILES
==================================================================================================*/
#include <stdint.h>
#include <stdbool.h>

/*==================================================================================================
*                                      DEFINES AND MACROS
==================================================================================================*/

/* UART Configuration */
#define HAL_UART_BAUDRATE           9600        /* Baud rate */
#define HAL_UART_DATA_BITS          8           /* 8-bit data */
#define HAL_UART_STOP_BITS          1           /* 1 stop bit */
#define HAL_UART_PARITY             0           /* No parity */

/* RX Buffer Configuration (for future use) */
#define HAL_UART_RX_BUFFER_SIZE     256

/*==================================================================================================
*                                      FUNCTION DECLARATIONS
==================================================================================================*/

/**
 * hal_uart_init()
 * 
 * Description:
 *   Initialize UART A1 peripheral
 *   Calls R_Config_UARTA1_Create() and R_Config_UARTA1_Start()
 *   
 * Parameters: None
 * 
 * Returns: None
 * 
 * Note:
 *   - Must be called before using any other UART functions
 *   - Configures 9600 baud, 8N1 (8 bits, no parity, 1 stop bit)
 */
void hal_uart_init(void);

/**
 * hal_uart_deinit()
 * 
 * Description:
 *   Disable and deinitialize UART A1 peripheral
 *   
 * Parameters: None
 * 
 * Returns: None
 */
void hal_uart_deinit(void);

/**
 * hal_uart_putchar(ch)
 * 
 * Description:
 *   Send a single character via UART
 *   Blocking function - waits for transmit complete
 *   
 * Parameters:
 *   ch - Character to send
 *   
 * Returns: None
 * 
 * Note:
 *   - Handles newline conversion: '\n' → '\r\n'
 *   - Blocking until transmit complete
 */
void hal_uart_putchar(char ch);

/**
 * hal_uart_puts(str)
 * 
 * Description:
 *   Send a null-terminated string via UART
 *   Calls hal_uart_putchar() for each character
 *   
 * Parameters:
 *   str - Pointer to null-terminated string
 *   
 * Returns: None
 * 
 * Note:
 *   - Handles newline conversion
 *   - String must be null-terminated
 */
void hal_uart_puts(const char *str);

/**
 * hal_uart_getchar()
 * 
 * Description:
 *   Receive a single character via UART
 *   Polling function - waits for data available
 *   
 * Parameters: None
 * 
 * Returns:
 *   Received character (uint8_t)
 *   
 * Note:
 *   - Blocking until data received
 *   - Does not return until valid data arrives
 */
uint8_t hal_uart_getchar(void);

/**
 * hal_uart_available()
 * 
 * Description:
 *   Check if data is available in RX buffer
 *   
 * Parameters: None
 * 
 * Returns:
 *   true  - Data available
 *   false - No data available
 *   
 * Note:
 *   - Non-blocking check
 *   - Can be used with RX interrupt if implemented
 */
bool hal_uart_available(void);

/**
 * hal_uart_send_buffer(buffer, length)
 * 
 * Description:
 *   Send multiple bytes via UART
 *   
 * Parameters:
 *   buffer - Pointer to data buffer
 *   length - Number of bytes to send
 *   
 * Returns: None
 * 
 * Note:
 *   - Blocking function
 */
void hal_uart_send_buffer(const uint8_t *buffer, uint16_t length);

/**
 * hal_uart_receive_buffer(buffer, length)
 * 
 * Description:
 *   Receive multiple bytes via UART
 *   
 * Parameters:
 *   buffer - Pointer to receive buffer
 *   length - Number of bytes to receive
 *   
 * Returns: None
 * 
 * Note:
 *   - Blocking function - waits for all bytes
 */
void hal_uart_receive_buffer(uint8_t *buffer, uint16_t length);

/**
 * hal_uart_printf_init()
 * 
 * Description:
 *   Redirect printf() output to UART
 *   Must be called after hal_uart_init()
 *   
 * Parameters: None
 * 
 * Returns: None
 * 
 * Note:
 *   - Required for printf() to work
 *   - Uses fputc() redirection internally
 */
void hal_uart_printf_init(void);

/*==================================================================================================
*                                      INLINE FUNCTIONS FOR LOW-LEVEL ACCESS
==================================================================================================*/

/**
 * hal_uart_is_tx_busy()
 * 
 * Description:
 *   Check if UART is busy transmitting
 *   
 * Returns:
 *   true  - Transmitting
 *   false - Idle
 */
bool hal_uart_is_tx_busy(void);

/**
 * hal_uart_is_rx_ready()
 * 
 * Description:
 *   Check if data is ready to read
 *   
 * Returns:
 *   true  - Data ready
 *   false - No data
 */
bool hal_uart_is_rx_ready(void);

/*==================================================================================================
*                                      PRINTF SUPPORT FUNCTIONS
==================================================================================================*/

/**
 * putchar(ch)
 * 
 * Description:
 *   Standard C library putchar() function
 *   Redirected to UART for printf() support
 *   
 * Parameters:
 *   ch - Character to output
 *   
 * Returns:
 *   The character written
 *   
 * Note:
 *   - Called by printf() internally
 *   - No need to call directly
 *   - Automatically redirects to UART
 *   - Respects log enable/disable setting
 */
int putchar(int ch);

/*==================================================================================================
*                                      LOG CONTROL FUNCTIONS
==================================================================================================*/

/**
 * hal_uart_log_enable()
 * 
 * Description:
 *   Enable UART logging (printf output)
 *   
 * Parameters: None
 * 
 * Returns: None
 * 
 * Note:
 *   - Logging is enabled by default
 *   - Use to re-enable after hal_uart_log_disable()
 */
void hal_uart_log_enable(void);

/**
 * hal_uart_log_disable()
 * 
 * Description:
 *   Disable UART logging (printf output will be suppressed)
 *   Direct UART functions (hal_uart_putchar, hal_uart_puts) still work
 *   
 * Parameters: None
 * 
 * Returns: None
 * 
 * Note:
 *   - printf() calls will be silently ignored
 *   - hal_uart_putchar() and hal_uart_puts() bypass this setting
 *   - Useful to silence test output
 */
void hal_uart_log_disable(void);

/**
 * hal_uart_log_is_enabled()
 * 
 * Description:
 *   Check if UART logging is currently enabled
 *   
 * Parameters: None
 * 
 * Returns:
 *   true  - Logging enabled
 *   false - Logging disabled
 */
bool hal_uart_log_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_H */
