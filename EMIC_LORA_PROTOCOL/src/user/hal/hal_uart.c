/*=====================================================================
 * HAL UART - UART Abstraction Layer Implementation
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

#include "hal_uart.h"
#include "Config_UARTA1.h"
#include <stdio.h>
#include <string.h>

/*==================================================================================================
*                                      STATIC VARIABLES
==================================================================================================*/

static volatile bool uart_initialized = false;
static volatile bool uart_log_enabled = true;    /* Log enabled by default */
static volatile uint8_t rx_buffer[HAL_UART_RX_BUFFER_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;
static volatile uint16_t rx_count = 0;

/*==================================================================================================
*                                      FUNCTION IMPLEMENTATIONS
==================================================================================================*/

/**
 * hal_uart_init()
 * Initialize UART A1 peripheral
 */
void hal_uart_init(void)
{
    if (uart_initialized)
    {
        return;  /* Already initialized */
    }
    
    /* Initialize UART A1 with Smart Configurator settings */
    R_Config_UARTA1_Create();
    
    /* Start UART transmission/reception */
    R_Config_UARTA1_Start();
    
    /* Clear RX buffer */
    rx_head = 0;
    rx_tail = 0;
    rx_count = 0;
    
    uart_initialized = true;
}

/**
 * hal_uart_deinit()
 * Disable and deinitialize UART A1 peripheral
 */
void hal_uart_deinit(void)
{
    if (!uart_initialized)
    {
        return;
    }
    
    /* Stop UART */
    R_Config_UARTA1_Stop();
    
    uart_initialized = false;
}

/**
 * hal_uart_putchar(ch)
 * Send a single character via UART
 */
void hal_uart_putchar(char ch)
{
    uint8_t data = (uint8_t)ch;
    
    /* Handle newline conversion: \n -> \r\n */
    if (ch == '\n')
    {
        /* Send carriage return first */
        uint8_t cr = '\r';
        R_Config_UARTA1_Send(&cr, 1);
        
        /* Small delay to ensure character is sent */
        volatile uint32_t i;
        for (i = 0; i < 100; i++)
        {
            /* Busy wait */
        }
    }
    
    /* Send the actual character */
    R_Config_UARTA1_Send(&data, 1);
    
    /* Small delay to ensure character is sent */
    volatile uint32_t i;
    for (i = 0; i < 100; i++)
    {
        /* Busy wait */
    }
}

/**
 * hal_uart_puts(str)
 * Send a null-terminated string via UART
 */
void hal_uart_puts(const char *str)
{
    if (str == NULL)
    {
        return;
    }
    
    while (*str != '\0')
    {
        hal_uart_putchar(*str);
        str++;
    }
}

/**
 * hal_uart_getchar()
 * Receive a single character via UART
 */
uint8_t hal_uart_getchar(void)
{
    uint8_t data;
    
    /* Poll until data is available */
    while (!hal_uart_available())
    {
        /* Wait for data */
    }
    
    /* Get data from RX buffer */
    data = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % HAL_UART_RX_BUFFER_SIZE;
    
    /* Disable interrupts to safely update count */
    DI();
    rx_count--;
    EI();
    
    return data;
}

/**
 * hal_uart_available()
 * Check if data is available in RX buffer
 */
bool hal_uart_available(void)
{
    return (rx_count > 0);
}

/**
 * hal_uart_send_buffer(buffer, length)
 * Send multiple bytes via UART
 */
void hal_uart_send_buffer(const uint8_t *buffer, uint16_t length)
{
    uint16_t i;
    
    if (buffer == NULL || length == 0)
    {
        return;
    }
    
    for (i = 0; i < length; i++)
    {
        hal_uart_putchar((char)buffer[i]);
    }
}

/**
 * hal_uart_receive_buffer(buffer, length)
 * Receive multiple bytes via UART
 */
void hal_uart_receive_buffer(uint8_t *buffer, uint16_t length)
{
    uint16_t i;
    
    if (buffer == NULL || length == 0)
    {
        return;
    }
    
    for (i = 0; i < length; i++)
    {
        buffer[i] = hal_uart_getchar();
    }
}

/**
 * hal_uart_is_tx_busy()
 * Check if UART is busy transmitting
 */
bool hal_uart_is_tx_busy(void)
{
    /* Check UART transmit status using CG driver */
    /* This would depend on the actual register layout */
    /* For now, return false (assuming fast transmission) */
    return false;
}

/**
 * hal_uart_is_rx_ready()
 * Check if data is ready to read
 */
bool hal_uart_is_rx_ready(void)
{
    return hal_uart_available();
}

/**
 * hal_uart_printf_init()
 * Redirect printf() output to UART
 */
void hal_uart_printf_init(void)
{
    /* This would require implementing a custom putchar() function */
    /* that redirects fputc() to UART */
    
    /* For embedded systems without proper newlib support, */
    /* we use a simpler approach: define a custom putchar() */
    
    /* The actual redirection is done via _write() or __putchar() */
    /* depending on the compiler and runtime environment */
}

/*==================================================================================================
*                                      PRINTF SUPPORT
==================================================================================================*/

/**
 * Custom putchar() implementation for printf() redirection
 * This function is called by printf() internally
 * Standard C library version
 */
int putchar(int ch)
{
    /* Only output if logging is enabled */
    if (uart_log_enabled)
    {
        hal_uart_putchar((char)ch);
    }
    return ch;
}

/**
 * _write() for Newlib (GCC ARM)
 * This function is called by printf() internally in some environments
 */
int _write(int file, const char *ptr, int len)
{
    int i;
    
    (void)file;  /* Unused parameter */
    
    for (i = 0; i < len; i++)
    {
        hal_uart_putchar(ptr[i]);
    }
    
    return len;
}

/**
 * Alternative: __putchar() for some compilers
 */
void __putchar(char ch)
{
    hal_uart_putchar(ch);
}

/**
 * Ensure stdout is not buffered
 * Required for printf() to work immediately
 */
void fflush_stdout(void)
{
    /* Already unbuffered by putchar implementation */
}

/*==================================================================================================
*                                      RECEIVE BUFFER MANAGEMENT (for future interrupt support)
==================================================================================================*/

/**
 * Internal function to add data to RX buffer
 * Called from RX interrupt handler (when implemented)
 */
void hal_uart_rx_buffer_add(uint8_t data)
{
    rx_buffer[rx_head] = data;
    rx_head = (rx_head + 1) % HAL_UART_RX_BUFFER_SIZE;
    
    /* Disable interrupts to safely update count */
    DI();
    if (rx_count < HAL_UART_RX_BUFFER_SIZE)
    {
        rx_count++;
    }
    else
    {
        /* Buffer overflow - overwrite oldest data */
        rx_tail = (rx_tail + 1) % HAL_UART_RX_BUFFER_SIZE;
    }
    EI();
}

/**
 * Clear RX buffer
 */
void hal_uart_rx_buffer_clear(void)
{
    DI();
    rx_head = 0;
    rx_tail = 0;
    rx_count = 0;
    EI();
}

/**
 * Get RX buffer count
 */
uint16_t hal_uart_rx_buffer_count(void)
{
    return rx_count;
}

/*==================================================================================================
*                                      LOG CONTROL FUNCTIONS
==================================================================================================*/

/**
 * hal_uart_log_enable()
 * Enable UART logging (printf output)
 */
void hal_uart_log_enable(void)
{
    uart_log_enabled = true;
}

/**
 * hal_uart_log_disable()
 * Disable UART logging (printf output will be suppressed)
 * Direct UART functions (hal_uart_putchar, hal_uart_puts) still work
 */
void hal_uart_log_disable(void)
{
    uart_log_enabled = false;
}

/**
 * hal_uart_log_is_enabled()
 * Check if UART logging is enabled
 * 
 * Returns:
 *   true  - Logging enabled
 *   false - Logging disabled
 */
bool hal_uart_log_is_enabled(void)
{
    return uart_log_enabled;
}
