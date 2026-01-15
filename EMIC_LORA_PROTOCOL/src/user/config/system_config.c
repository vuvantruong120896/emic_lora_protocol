/**
 * @file system_config.c
 * @brief System configuration constants implementation.
 * @details Provides provisioned device identity, encryption keys, and firmware version.
 *          All values are placeholders: replace during device provisioning with values
 *          from flash, eFuse, or NV store.
 *
 *          **Note:** This file sits at system layer (Layer 2-3) and provides constants
 *          used by radio, MAC, and protocol layers.
 *
 * @author EMIC Team
 * @version 1.0.0
 * @date 2026-01-14
 */

#include "system_config.h"

/** @brief Device encryption key (placeholder; 16 bytes). Replace during provisioning. */
const unsigned char SYSTEM_DEV_KEY[16] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F
};

/** @brief Network shared broadcast key (placeholder; 16 bytes). Replace during provisioning. */
const unsigned char SYSTEM_GROUP_KEY[16] = {
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F
};

/** @brief Device PAN ID / Network ID (placeholder; 6 bytes, big-endian). Replace during provisioning. */
const unsigned char SYSTEM_PAN_ID[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
/** @brief Device serial/ED number (placeholder; 6 bytes, unique per device). Replace during provisioning. */
const unsigned char SYSTEM_SERI_ED[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };

/** @brief Device firmware version (major=1, minor=1, patch=2). */
const unsigned char SYSTEM_FIRM_ID[3] = { 1, 1, 2 };
