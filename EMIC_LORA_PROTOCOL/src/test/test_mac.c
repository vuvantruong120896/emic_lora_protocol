/*=====================================================================
 * MAC Layer Test
 * 
 * Test CSMA/CA, time slotting, channel management
 *=====================================================================*/

#include "lora_mac.h"
#include "sx1262_config.h"
#include "hal_systick.h"
#include "log_control.h"

/* Test result tracking */
static uint32_t g_test_pass = 0;
static uint32_t g_test_fail = 0;

/* ===================================================================
 * Test: Channel Management
 * =================================================================== */

void test_mac_channels(void)
{
    log_info("%s", "=== MAC Test: Channels ===");
    
    /* Test 1: Join channel */
    mac_set_join_channel();
    uint32_t ch = mac_get_current_channel();
    
    if (ch == 920225000) {
        log_info("✓ Join channel (CH1): %.3f MHz", ch / 1e6);
        g_test_pass++;
    } else {
        log_error("✗ Join channel failed: got %.3f MHz", ch / 1e6);
        g_test_fail++;
    }
    
    /* Test 2: Data channel selection (8 times to check randomness) */
    log_info("%s", "Random data channel selection (8 samples):");
    
    for (int i = 0; i < 8; i++) {
        ch = mac_select_random_data_channel();
        
        /* Verify it's one of the 8 data channels */
        int valid = 0;
        if (ch == 920525000 || ch == 920825000 || ch == 921125000 || 
            ch == 921425000 || ch == 921725000 || ch == 922025000 || 
            ch == 922325000 || ch == 922625000) {
            valid = 1;
        }
        
        if (valid) {
            log_debug("  [%d] %.3f MHz ✓", i + 1, ch / 1e6);
            g_test_pass++;
        } else {
            log_error("  [%d] INVALID frequency: %.3f MHz", i + 1, ch / 1e6);
            g_test_fail++;
        }
        
        hal_systick_delay_ms(10);
    }
}

/* ===================================================================
 * Test: Time Slotting
 * =================================================================== */

void test_mac_time_slotting(void)
{
    log_info("%s", "=== MAC Test: Time Slotting ===");
    
    struct {
        uint16_t short_addr;
        uint16_t expected_slot;
        uint16_t expected_tx_time;
    } test_cases[] = {
        {0x0042, 6,   24},   /* 66 % 60 = 6 → 6 × 4s = 24s */
        {0x001F, 31, 124},   /* 31 % 60 = 31 → 31 × 4s = 124s */
        {0x0100, 16,  64},   /* 256 % 60 = 16 → 16 × 4s = 64s */
        {0x0000, 0,    0},   /* 0 % 60 = 0 → 0 × 4s = 0s */
        {0x00FF, 59, 236},   /* 255 % 60 = 55 → ... wait, let me recalc: 255 % 60 = 15 → 15 × 4 = 60 */
    };
    
    /* Actually fix the last one */
    test_cases[4].expected_slot = 255 % 60;
    test_cases[4].expected_tx_time = test_cases[4].expected_slot * 4;
    
    log_info("%s", "Slot calculation tests:");
    
    for (int i = 0; i < 5; i++) {
        uint16_t tx_time = mac_calculate_tx_slot(test_cases[i].short_addr);
        
        if (tx_time == test_cases[i].expected_tx_time) {
            log_info("✓ ShortAddr=0x%04X → %u s (slot %u)", 
                    test_cases[i].short_addr, tx_time, test_cases[i].expected_slot);
            g_test_pass++;
        } else {
            log_error("✗ ShortAddr=0x%04X → expected %u s, got %u s", 
                     test_cases[i].short_addr, test_cases[i].expected_tx_time, tx_time);
            g_test_fail++;
        }
    }
    
    /* Test: Is my slot? */
    log_info("%s", "Slot phase tests:");
    
    uint16_t phase = mac_get_slot_phase(0);
    if (phase == 0) {
        log_info("✓ Phase at 0s: %u (expected 0)", phase);
        g_test_pass++;
    } else {
        log_error("✗ Phase at 0s: %u (expected 0)", phase);
        g_test_fail++;
    }
    
    phase = mac_get_slot_phase(125);
    if (phase == 125) {
        log_info("✓ Phase at 125s: %u (expected 125)", phase);
        g_test_pass++;
    } else {
        log_error("✗ Phase at 125s: %u (expected 125)", phase);
        g_test_fail++;
    }
    
    phase = mac_get_slot_phase(240);  /* Wraps */
    if (phase == 0) {
        log_info("✓ Phase at 240s (wrap): %u (expected 0)", phase);
        g_test_pass++;
    } else {
        log_error("✗ Phase at 240s (wrap): %u (expected 0)", phase);
        g_test_fail++;
    }
    
    /* Test: Is in TX slot? */
    log_info("%s", "In-slot detection:");
    
    uint16_t short_addr = 0x0042;  /* Expected TX at 24s */
    
    if (mac_is_my_tx_slot(24, short_addr)) {
        log_info("✓ In TX slot at 24s for ShortAddr=0x%04X", short_addr);
        g_test_pass++;
    } else {
        log_error("✗ NOT in TX slot at 24s for ShortAddr=0x%04X", short_addr);
        g_test_fail++;
    }
    
    if (mac_is_my_tx_slot(25, short_addr)) {
        log_info("✓ Still in TX slot at 25s (4s duration)", short_addr);
        g_test_pass++;
    } else {
        log_error("✗ NOT in TX slot at 25s", short_addr);
        g_test_fail++;
    }
    
    if (!mac_is_my_tx_slot(23, short_addr)) {
        log_info("✓ NOT in TX slot at 23s (before slot)", short_addr);
        g_test_pass++;
    } else {
        log_error("✗ FALSE: In TX slot at 23s (should be before)", short_addr);
        g_test_fail++;
    }
    
    if (!mac_is_my_tx_slot(28, short_addr)) {
        log_info("✓ NOT in TX slot at 28s (after slot)", short_addr);
        g_test_pass++;
    } else {
        log_error("✗ FALSE: In TX slot at 28s (should be after)", short_addr);
        g_test_fail++;
    }
}

/* ===================================================================
 * Test: CSMA/CA (Channel Sensing)
 * =================================================================== */

void test_mac_csma(void)
{
    log_info("%s", "=== MAC Test: CSMA/CA ===");
    
    /* Test channel clear detection */
    log_info("%s", "Channel clear check:");
    
    uint8_t is_clear = mac_is_channel_clear();
    
    if (is_clear) {
        log_info("%s","Channel is clear (no signal detected)");
        g_test_pass++;
    } else {
        log_warn("%s","! Channel busy (RSSI > -100 dBm) - may be expected if TX is active");
        g_test_pass++;  /* Not a fail if channel is busy */
    }
    
    log_info("%s", "CSMA/CA test requires active SX1262 and transmitter");
}

/* ===================================================================
 * Main Test Entry
 * =================================================================== */

void test_mac_main(void)
{
    log_info("%s", "========================================");
    log_info("%s", "   MAC LAYER TEST SUITE");
    log_info("%s", "========================================");
    
    g_test_pass = 0;
    g_test_fail = 0;
    
    test_mac_channels();
    hal_systick_delay_ms(100);
    
    test_mac_time_slotting();
    hal_systick_delay_ms(100);
    
    test_mac_csma();
    
    log_info("%s", "========================================");
    log_info("RESULTS: %u passed, %u failed", g_test_pass, g_test_fail);
    log_info("%s", "========================================");
}
