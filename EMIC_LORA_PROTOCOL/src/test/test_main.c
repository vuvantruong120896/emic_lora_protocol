#include "test_sx126x.h"
#include "test_uart_debug.h"
#include "log_control.h"
#include "hal_systick.h"
#include "hal_uart.h"
#include "hal_spi.h"

/* HAL Layer Tests */
extern void main_test_spi(void);
extern void main_test_gpio(void);
extern void main_test_systick(void);
extern void test_uart_all(void);

/* PHY Layer Tests */
extern void test_sx126x_main(void);

/* MAC Layer Tests */
extern void test_mac_main(void);

/* Protocol Layer Tests */
extern void test_protocol_main(void);

/* Utilities Tests */
extern void test_utils_main(void);

/* Test suite selection */
#define RUN_HAL_TESTS       0
#define RUN_PHY_TESTS       1
#define RUN_PHY_INIT_CONFIG 1    /* Section 2.2 */
#define RUN_PHY_CHANNELS    1    /* Section 2.2 */
// #define RUN_PHY_TX          1    /* Section 2.3 */
#define RUN_PHY_RX          1    /* Section 2.4 */
#define RUN_MAC_TESTS       0
#define RUN_PROTOCOL_TESTS  0
#define RUN_UTILS_TESTS     0

void main_test_entry(void)
{
    /* Initialize HAL and logging */
    hal_systick_init();
    hal_uart_init();
    log_set_level(LOG_LEVEL_DEBUG);
    hal_spi_init();

    log_info("%s", "================================================");
    log_info("%s", "      EMIC LORASAFE TEST SUITE");
    log_info("%s", "================================================");
    log_info("%s","Date: December 25, 2025");
    log_info("%s","Layers: HAL -> PHY -> MAC -> Protocol -> Application");
    log_info("%s", "================================================");
    
    #if RUN_HAL_TESTS
    log_info("%s", ">>> Running HAL Layer Tests <<<");
    // main_test_spi();
    // main_test_gpio();
    // main_test_systick();
    // test_uart_all();
    #endif
    
    #if RUN_PHY_TESTS
    log_info("%s", ">>> Running PHY Layer Tests <<<");
    test_sx126x_main();
    
    #if RUN_PHY_INIT_CONFIG
    test_sx1262_init_config();
    #endif
    
    #if RUN_PHY_CHANNELS
    test_sx1262_channels();
    #endif
    
    #if RUN_PHY_TX
    test_sx1262_tx();
    #endif
    
    #if RUN_PHY_RX
    test_sx1262_rx();
    #endif
    
    #endif
    
    #if RUN_MAC_TESTS
    log_info("%s", ">>> Running MAC Layer Tests <<<");
    test_mac_main();
    log_info("");
    #endif
    
    #if RUN_PROTOCOL_TESTS
    log_info("%s", ">>> Running Protocol Layer Tests <<<");
    test_protocol_main();
    log_info("");
    #endif
    
    #if RUN_UTILS_TESTS
    log_info("%s", ">>> Running Utilities Tests <<<");
    test_utils_main();
    log_info("");
    #endif
    
    log_info("%s", "================================================");
    log_info("%s", "      TEST SUITE COMPLETE");
    log_info("%s", "================================================");

    while (1)
    {
        /* Idle */
    }
    
}
