#include "nv_store.h"

/* Data Flash-backed storage (RFD via Smart Config smc_gen).
 * Policy:
 * - Keep hot counters in RAM and checkpoint occasionally to reduce erase/wear.
 * - Store latest record with CRC + monotonic sequence in two alternating DF blocks.
 */

#include "../hal/hal_dataflash.h"
#include "../utils/crc16.h"

#include <string.h>

#define NV_STORE_DF_MAGIC           (0x5453564EU) /* 'NVST' little-endian */
#define NV_STORE_DF_VERSION_V1      (1U)
#define NV_STORE_DF_VERSION_V2      (2U)
#define NV_STORE_DF_VERSION_V3      (3U)
#define NV_STORE_DF_VERSION_V4      (4U)
#define NV_STORE_DF_VERSION_V5      (5U)
#define NV_STORE_DF_VERSION_V6      (6U)

#define NV_STORE_DF_BLOCK_A         (0U)
#define NV_STORE_DF_BLOCK_B         (1U)

/* Write FCnt checkpoint every 32 increments (tunable). */
#define NV_STORE_FCNT_CHECKPOINT_MASK (0x1FU)

/* Device config defaults. These are persisted values; 0/0x8000 indicates "unset".
 * Runtime may fall back to app_config.h defaults if unset.
 */
#define NV_STORE_CFG_RSSI_UNSET_DBM     ((int16_t)0x8000)
#define NV_STORE_CFG_HB_UNSET_S         ((uint16_t)0U)
#define NV_STORE_CFG_SENS_UNSET         ((uint16_t)0U)

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t seq;
    uint32_t fcnt_up;
    uint16_t last_alarm_id;
    uint16_t crc16;
} nv_store_df_record_v1_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t seq;
    uint32_t fcnt_up;
    uint16_t last_alarm_id;
    uint8_t lora_channel_idx;
    uint8_t reserved[3];
    uint16_t crc16;
} nv_store_df_record_v2_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t seq;
    uint32_t fcnt_up;
    uint16_t last_alarm_id;
    uint8_t lora_channel_idx;
    uint8_t pan_id[6];
    uint8_t seri_ed[6];
    uint8_t reserved[3];
    uint16_t crc16;
} nv_store_df_record_v3_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t seq;
    uint32_t fcnt_up;
    uint32_t fcnt_down;
    uint16_t last_alarm_id;
    uint8_t lora_channel_idx;
    uint8_t pan_id[6];
    uint8_t seri_ed[6];
    uint32_t fire_start_epoch_s;
    uint8_t reserved[3];
    uint16_t crc16;
} nv_store_df_record_v4_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t seq;
    uint32_t fcnt_up;
    uint32_t fcnt_down;
    uint16_t last_alarm_id;
    uint8_t lora_channel_idx;
    uint8_t pan_id[6];
    uint8_t seri_ed[6];
    uint32_t fire_start_epoch_s;
    int16_t lora_rssi_threshold_dbm;
    uint16_t heartbeat_period_s;
    uint8_t smoke_sensitivity;
    uint8_t heat_sensitivity;
    uint8_t reserved[1];
    uint16_t crc16;
} nv_store_df_record_v5_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t seq;
    uint32_t fcnt_up;
    uint32_t fcnt_down;
    uint16_t last_alarm_id;
    uint8_t lora_channel_idx;
    uint8_t pan_id[6];
    uint8_t seri_ed[6];
    uint32_t fire_start_epoch_s;
    int16_t lora_rssi_threshold_dbm;
    uint16_t heartbeat_period_s;
    uint16_t smoke_sensitivity;
    uint16_t heat_sensitivity;
    uint8_t reserved[3];
    uint16_t crc16;
} nv_store_df_record_v6_t;

static uint32_t s_fcnt_up;
static uint32_t s_fcnt_down;
static uint16_t s_last_alarm_id;
static uint8_t s_lora_channel_idx;
static uint8_t s_pan_id[6];
static uint8_t s_seri_ed[6];
static uint32_t s_fire_start_epoch_s;
static int16_t s_lora_rssi_threshold_dbm;
static uint16_t s_heartbeat_period_s;
static uint16_t s_smoke_sensitivity;
static uint16_t s_heat_sensitivity;

static uint8_t s_df_ready;
static uint8_t s_df_active_block;
static uint32_t s_df_seq;

static uint16_t nv_store_df_crc16(const void *rec, uint16_t len_without_crc)
{
    return crc16_calculate((const uint8_t *)rec, len_without_crc);
}

static uint8_t nv_store_df_record_v1_is_valid(const nv_store_df_record_v1_t *rec)
{
    if (rec->magic != NV_STORE_DF_MAGIC)
    {
        return 0U;
    }

    if (rec->version != NV_STORE_DF_VERSION_V1)
    {
        return 0U;
    }

    if (rec->length != (uint16_t)sizeof(nv_store_df_record_v1_t))
    {
        return 0U;
    }

    return (nv_store_df_crc16(rec, (uint16_t)(sizeof(nv_store_df_record_v1_t) - sizeof(rec->crc16))) == rec->crc16) ? 1U : 0U;
}

static uint8_t nv_store_df_record_v2_is_valid(const nv_store_df_record_v2_t *rec)
{
    if (rec->magic != NV_STORE_DF_MAGIC)
    {
        return 0U;
    }

    if (rec->version != NV_STORE_DF_VERSION_V2)
    {
        return 0U;
    }

    if (rec->length != (uint16_t)sizeof(nv_store_df_record_v2_t))
    {
        return 0U;
    }

    return (nv_store_df_crc16(rec, (uint16_t)(sizeof(nv_store_df_record_v2_t) - sizeof(rec->crc16))) == rec->crc16) ? 1U : 0U;
}

static uint8_t nv_store_df_record_v3_is_valid(const nv_store_df_record_v3_t *rec)
{
    if (rec->magic != NV_STORE_DF_MAGIC)
    {
        return 0U;
    }

    if (rec->version != NV_STORE_DF_VERSION_V3)
    {
        return 0U;
    }

    if (rec->length != (uint16_t)sizeof(nv_store_df_record_v3_t))
    {
        return 0U;
    }

    return (nv_store_df_crc16(rec, (uint16_t)(sizeof(nv_store_df_record_v3_t) - sizeof(rec->crc16))) == rec->crc16) ? 1U : 0U;
}

static uint8_t nv_store_df_record_v4_is_valid(const nv_store_df_record_v4_t *rec)
{
    if (rec->magic != NV_STORE_DF_MAGIC)
    {
        return 0U;
    }

    if (rec->version != NV_STORE_DF_VERSION_V4)
    {
        return 0U;
    }

    if (rec->length != (uint16_t)sizeof(nv_store_df_record_v4_t))
    {
        return 0U;
    }

    return (nv_store_df_crc16(rec, (uint16_t)(sizeof(nv_store_df_record_v4_t) - sizeof(rec->crc16))) == rec->crc16) ? 1U : 0U;
}

static uint8_t nv_store_df_record_v5_is_valid(const nv_store_df_record_v5_t *rec)
{
    if (rec->magic != NV_STORE_DF_MAGIC)
    {
        return 0U;
    }

    if (rec->version != NV_STORE_DF_VERSION_V5)
    {
        return 0U;
    }

    if (rec->length != (uint16_t)sizeof(nv_store_df_record_v5_t))
    {
        return 0U;
    }

    return (nv_store_df_crc16(rec, (uint16_t)(sizeof(nv_store_df_record_v5_t) - sizeof(rec->crc16))) == rec->crc16) ? 1U : 0U;
}

static uint8_t nv_store_df_record_v6_is_valid(const nv_store_df_record_v6_t *rec)
{
    if (rec->magic != NV_STORE_DF_MAGIC)
    {
        return 0U;
    }

    if (rec->version != NV_STORE_DF_VERSION_V6)
    {
        return 0U;
    }

    if (rec->length != (uint16_t)sizeof(nv_store_df_record_v6_t))
    {
        return 0U;
    }

    return (nv_store_df_crc16(rec, (uint16_t)(sizeof(nv_store_df_record_v6_t) - sizeof(rec->crc16))) == rec->crc16) ? 1U : 0U;
}

static void nv_store_df_read_block_raw(uint8_t block_number, uint8_t *out, uint16_t len)
{
    uint32_t addr = hal_dataflash_block_start_addr(block_number);
    hal_dataflash_read(addr, out, len);
}

static uint8_t nv_store_df_pick_active_block_v2(const nv_store_df_record_v2_t *a, const nv_store_df_record_v2_t *b)
{
    uint8_t a_ok = nv_store_df_record_v2_is_valid(a);
    uint8_t b_ok = nv_store_df_record_v2_is_valid(b);

    if ((a_ok == 0U) && (b_ok == 0U))
    {
        return 0xFFU;
    }

    if (a_ok != 0U && b_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_A;
    }

    if (b_ok != 0U && a_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_B;
    }

    return (b->seq >= a->seq) ? NV_STORE_DF_BLOCK_B : NV_STORE_DF_BLOCK_A;
}

static uint8_t nv_store_df_pick_active_block_v3(const nv_store_df_record_v3_t *a, const nv_store_df_record_v3_t *b)
{
    uint8_t a_ok = nv_store_df_record_v3_is_valid(a);
    uint8_t b_ok = nv_store_df_record_v3_is_valid(b);

    if ((a_ok == 0U) && (b_ok == 0U))
    {
        return 0xFFU;
    }

    if (a_ok != 0U && b_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_A;
    }

    if (b_ok != 0U && a_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_B;
    }

    return (b->seq >= a->seq) ? NV_STORE_DF_BLOCK_B : NV_STORE_DF_BLOCK_A;
}

static uint8_t nv_store_df_pick_active_block_v4(const nv_store_df_record_v4_t *a, const nv_store_df_record_v4_t *b)
{
    uint8_t a_ok = nv_store_df_record_v4_is_valid(a);
    uint8_t b_ok = nv_store_df_record_v4_is_valid(b);

    if ((a_ok == 0U) && (b_ok == 0U))
    {
        return 0xFFU;
    }

    if (a_ok != 0U && b_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_A;
    }

    if (b_ok != 0U && a_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_B;
    }

    return (b->seq >= a->seq) ? NV_STORE_DF_BLOCK_B : NV_STORE_DF_BLOCK_A;
}

static uint8_t nv_store_df_pick_active_block_v5(const nv_store_df_record_v5_t *a, const nv_store_df_record_v5_t *b)
{
    uint8_t a_ok = nv_store_df_record_v5_is_valid(a);
    uint8_t b_ok = nv_store_df_record_v5_is_valid(b);

    if ((a_ok == 0U) && (b_ok == 0U))
    {
        return 0xFFU;
    }

    if (a_ok != 0U && b_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_A;
    }

    if (b_ok != 0U && a_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_B;
    }

    return (b->seq >= a->seq) ? NV_STORE_DF_BLOCK_B : NV_STORE_DF_BLOCK_A;
}

static uint8_t nv_store_df_pick_active_block_v6(const nv_store_df_record_v6_t *a, const nv_store_df_record_v6_t *b)
{
    uint8_t a_ok = nv_store_df_record_v6_is_valid(a);
    uint8_t b_ok = nv_store_df_record_v6_is_valid(b);

    if ((a_ok == 0U) && (b_ok == 0U))
    {
        return 0xFFU;
    }

    if (a_ok != 0U && b_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_A;
    }

    if (b_ok != 0U && a_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_B;
    }

    return (b->seq >= a->seq) ? NV_STORE_DF_BLOCK_B : NV_STORE_DF_BLOCK_A;
}

static uint8_t nv_store_df_pick_active_block_v1(const nv_store_df_record_v1_t *a, const nv_store_df_record_v1_t *b)
{
    uint8_t a_ok = nv_store_df_record_v1_is_valid(a);
    uint8_t b_ok = nv_store_df_record_v1_is_valid(b);

    if ((a_ok == 0U) && (b_ok == 0U))
    {
        return 0xFFU;
    }

    if (a_ok != 0U && b_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_A;
    }

    if (b_ok != 0U && a_ok == 0U)
    {
        return NV_STORE_DF_BLOCK_B;
    }

    return (b->seq >= a->seq) ? NV_STORE_DF_BLOCK_B : NV_STORE_DF_BLOCK_A;
}

static uint8_t nv_store_df_write_record(uint8_t block_number, const uint8_t *rec, uint16_t len)
{
    hal_dataflash_result_t r;
    uint32_t addr = hal_dataflash_block_start_addr(block_number);

    r = hal_dataflash_erase_block(block_number);
    if (r != HAL_DATAFLASH_OK)
    {
        return 0U;
    }

    r = hal_dataflash_write(addr, rec, len);
    if (r != HAL_DATAFLASH_OK)
    {
        return 0U;
    }

    return 1U;
}

static void nv_store_df_commit(uint8_t force)
{
    if (s_df_ready == 0U)
    {
        return;
    }

    if (force == 0U)
    {
        /* Throttle writes: FCnt checkpoints only, and immediate commits for infrequent values. */
        if (((s_fcnt_up & NV_STORE_FCNT_CHECKPOINT_MASK) != 0U) && ((s_fcnt_down & NV_STORE_FCNT_CHECKPOINT_MASK) != 0U))
        {
            return;
        }
    }

    {
        nv_store_df_record_v6_t rec;
        uint8_t target = (s_df_active_block == NV_STORE_DF_BLOCK_A) ? NV_STORE_DF_BLOCK_B : NV_STORE_DF_BLOCK_A;

        rec.magic = NV_STORE_DF_MAGIC;
        rec.version = NV_STORE_DF_VERSION_V6;
        rec.length = (uint16_t)sizeof(nv_store_df_record_v6_t);
        rec.seq = s_df_seq + 1UL;
        rec.fcnt_up = s_fcnt_up;
        rec.fcnt_down = s_fcnt_down;
        rec.last_alarm_id = s_last_alarm_id;
        rec.lora_channel_idx = s_lora_channel_idx;
        memcpy(rec.pan_id, s_pan_id, sizeof(rec.pan_id));
        memcpy(rec.seri_ed, s_seri_ed, sizeof(rec.seri_ed));
        rec.fire_start_epoch_s = s_fire_start_epoch_s;
        rec.lora_rssi_threshold_dbm = s_lora_rssi_threshold_dbm;
        rec.heartbeat_period_s = s_heartbeat_period_s;
        rec.smoke_sensitivity = s_smoke_sensitivity;
        rec.heat_sensitivity = s_heat_sensitivity;
        rec.reserved[0] = 0U;
        rec.reserved[1] = 0U;
        rec.reserved[2] = 0U;
        rec.crc16 = nv_store_df_crc16(&rec, (uint16_t)(sizeof(nv_store_df_record_v6_t) - sizeof(rec.crc16)));

        if (nv_store_df_write_record(target, (const uint8_t *)&rec, (uint16_t)sizeof(nv_store_df_record_v6_t)) != 0U)
        {
            s_df_active_block = target;
            s_df_seq = rec.seq;
        }
    }
}

void nv_store_init(void)
{
    nv_store_df_record_v6_t rec_a6;
    nv_store_df_record_v6_t rec_b6;
    nv_store_df_record_v5_t rec_a5;
    nv_store_df_record_v5_t rec_b5;
    nv_store_df_record_v4_t rec_a4;
    nv_store_df_record_v4_t rec_b4;
    nv_store_df_record_v3_t rec_a3;
    nv_store_df_record_v3_t rec_b3;
    nv_store_df_record_v2_t rec_a2;
    nv_store_df_record_v2_t rec_b2;
    nv_store_df_record_v1_t rec_a1;
    nv_store_df_record_v1_t rec_b1;

    s_df_ready = 0U;
    s_df_active_block = NV_STORE_DF_BLOCK_A;
    s_df_seq = 0UL;

    if (hal_dataflash_init() == HAL_DATAFLASH_OK)
    {
        s_df_ready = 1U;
    }

    if (s_df_ready != 0U)
    {
        uint8_t active;

        /* Try V6 first. */
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_A, (uint8_t *)&rec_a6, (uint16_t)sizeof(rec_a6));
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_B, (uint8_t *)&rec_b6, (uint16_t)sizeof(rec_b6));
        active = nv_store_df_pick_active_block_v6(&rec_a6, &rec_b6);
        if (active == NV_STORE_DF_BLOCK_A)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_A;
            s_df_seq = rec_a6.seq;
            s_fcnt_up = rec_a6.fcnt_up;
            s_fcnt_down = rec_a6.fcnt_down;
            s_last_alarm_id = rec_a6.last_alarm_id;
            s_lora_channel_idx = rec_a6.lora_channel_idx;
            memcpy(s_pan_id, rec_a6.pan_id, sizeof(s_pan_id));
            memcpy(s_seri_ed, rec_a6.seri_ed, sizeof(s_seri_ed));
            s_fire_start_epoch_s = rec_a6.fire_start_epoch_s;
            s_lora_rssi_threshold_dbm = rec_a6.lora_rssi_threshold_dbm;
            s_heartbeat_period_s = rec_a6.heartbeat_period_s;
            s_smoke_sensitivity = rec_a6.smoke_sensitivity;
            s_heat_sensitivity = rec_a6.heat_sensitivity;
            return;
        }
        else if (active == NV_STORE_DF_BLOCK_B)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_B;
            s_df_seq = rec_b6.seq;
            s_fcnt_up = rec_b6.fcnt_up;
            s_fcnt_down = rec_b6.fcnt_down;
            s_last_alarm_id = rec_b6.last_alarm_id;
            s_lora_channel_idx = rec_b6.lora_channel_idx;
            memcpy(s_pan_id, rec_b6.pan_id, sizeof(s_pan_id));
            memcpy(s_seri_ed, rec_b6.seri_ed, sizeof(s_seri_ed));
            s_fire_start_epoch_s = rec_b6.fire_start_epoch_s;
            s_lora_rssi_threshold_dbm = rec_b6.lora_rssi_threshold_dbm;
            s_heartbeat_period_s = rec_b6.heartbeat_period_s;
            s_smoke_sensitivity = rec_b6.smoke_sensitivity;
            s_heat_sensitivity = rec_b6.heat_sensitivity;
            return;
        }

        /* Try V5 first. */
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_A, (uint8_t *)&rec_a5, (uint16_t)sizeof(rec_a5));
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_B, (uint8_t *)&rec_b5, (uint16_t)sizeof(rec_b5));
        active = nv_store_df_pick_active_block_v5(&rec_a5, &rec_b5);
        if (active == NV_STORE_DF_BLOCK_A)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_A;
            s_df_seq = rec_a5.seq;
            s_fcnt_up = rec_a5.fcnt_up;
            s_fcnt_down = rec_a5.fcnt_down;
            s_last_alarm_id = rec_a5.last_alarm_id;
            s_lora_channel_idx = rec_a5.lora_channel_idx;
            memcpy(s_pan_id, rec_a5.pan_id, sizeof(s_pan_id));
            memcpy(s_seri_ed, rec_a5.seri_ed, sizeof(s_seri_ed));
            s_fire_start_epoch_s = rec_a5.fire_start_epoch_s;
            s_lora_rssi_threshold_dbm = rec_a5.lora_rssi_threshold_dbm;
            s_heartbeat_period_s = rec_a5.heartbeat_period_s;
            s_smoke_sensitivity = (uint16_t)rec_a5.smoke_sensitivity;
            s_heat_sensitivity = (uint16_t)rec_a5.heat_sensitivity;
            nv_store_df_commit(1U);
            return;
        }
        else if (active == NV_STORE_DF_BLOCK_B)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_B;
            s_df_seq = rec_b5.seq;
            s_fcnt_up = rec_b5.fcnt_up;
            s_fcnt_down = rec_b5.fcnt_down;
            s_last_alarm_id = rec_b5.last_alarm_id;
            s_lora_channel_idx = rec_b5.lora_channel_idx;
            memcpy(s_pan_id, rec_b5.pan_id, sizeof(s_pan_id));
            memcpy(s_seri_ed, rec_b5.seri_ed, sizeof(s_seri_ed));
            s_fire_start_epoch_s = rec_b5.fire_start_epoch_s;
            s_lora_rssi_threshold_dbm = rec_b5.lora_rssi_threshold_dbm;
            s_heartbeat_period_s = rec_b5.heartbeat_period_s;
            s_smoke_sensitivity = (uint16_t)rec_b5.smoke_sensitivity;
            s_heat_sensitivity = (uint16_t)rec_b5.heat_sensitivity;
            nv_store_df_commit(1U);
            return;
        }

        /* Try V4 first. */
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_A, (uint8_t *)&rec_a4, (uint16_t)sizeof(rec_a4));
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_B, (uint8_t *)&rec_b4, (uint16_t)sizeof(rec_b4));
        active = nv_store_df_pick_active_block_v4(&rec_a4, &rec_b4);
        if (active == NV_STORE_DF_BLOCK_A)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_A;
            s_df_seq = rec_a4.seq;
            s_fcnt_up = rec_a4.fcnt_up;
            s_fcnt_down = rec_a4.fcnt_down;
            s_last_alarm_id = rec_a4.last_alarm_id;
            s_lora_channel_idx = rec_a4.lora_channel_idx;
            memcpy(s_pan_id, rec_a4.pan_id, sizeof(s_pan_id));
            memcpy(s_seri_ed, rec_a4.seri_ed, sizeof(s_seri_ed));
            s_fire_start_epoch_s = rec_a4.fire_start_epoch_s;
            s_lora_rssi_threshold_dbm = NV_STORE_CFG_RSSI_UNSET_DBM;
            s_heartbeat_period_s = NV_STORE_CFG_HB_UNSET_S;
            s_smoke_sensitivity = NV_STORE_CFG_SENS_UNSET;
            s_heat_sensitivity = NV_STORE_CFG_SENS_UNSET;
            nv_store_df_commit(1U);
            return;
        }
        else if (active == NV_STORE_DF_BLOCK_B)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_B;
            s_df_seq = rec_b4.seq;
            s_fcnt_up = rec_b4.fcnt_up;
            s_fcnt_down = rec_b4.fcnt_down;
            s_last_alarm_id = rec_b4.last_alarm_id;
            s_lora_channel_idx = rec_b4.lora_channel_idx;
            memcpy(s_pan_id, rec_b4.pan_id, sizeof(s_pan_id));
            memcpy(s_seri_ed, rec_b4.seri_ed, sizeof(s_seri_ed));
            s_fire_start_epoch_s = rec_b4.fire_start_epoch_s;
            s_lora_rssi_threshold_dbm = NV_STORE_CFG_RSSI_UNSET_DBM;
            s_heartbeat_period_s = NV_STORE_CFG_HB_UNSET_S;
            s_smoke_sensitivity = NV_STORE_CFG_SENS_UNSET;
            s_heat_sensitivity = NV_STORE_CFG_SENS_UNSET;
            nv_store_df_commit(1U);
            return;
        }

        /* Try V3 first. */
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_A, (uint8_t *)&rec_a3, (uint16_t)sizeof(rec_a3));
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_B, (uint8_t *)&rec_b3, (uint16_t)sizeof(rec_b3));
        active = nv_store_df_pick_active_block_v3(&rec_a3, &rec_b3);
        if (active == NV_STORE_DF_BLOCK_A)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_A;
            s_df_seq = rec_a3.seq;
            s_fcnt_up = rec_a3.fcnt_up;
            s_fcnt_down = 1UL;
            s_last_alarm_id = rec_a3.last_alarm_id;
            s_lora_channel_idx = rec_a3.lora_channel_idx;
            memcpy(s_pan_id, rec_a3.pan_id, sizeof(s_pan_id));
            memcpy(s_seri_ed, rec_a3.seri_ed, sizeof(s_seri_ed));
            s_fire_start_epoch_s = 0UL;
            nv_store_df_commit(1U);
            return;
        }
        else if (active == NV_STORE_DF_BLOCK_B)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_B;
            s_df_seq = rec_b3.seq;
            s_fcnt_up = rec_b3.fcnt_up;
            s_fcnt_down = 1UL;
            s_last_alarm_id = rec_b3.last_alarm_id;
            s_lora_channel_idx = rec_b3.lora_channel_idx;
            memcpy(s_pan_id, rec_b3.pan_id, sizeof(s_pan_id));
            memcpy(s_seri_ed, rec_b3.seri_ed, sizeof(s_seri_ed));
            s_fire_start_epoch_s = 0UL;
            nv_store_df_commit(1U);
            return;
        }

        /* Try V2 first. */
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_A, (uint8_t *)&rec_a2, (uint16_t)sizeof(rec_a2));
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_B, (uint8_t *)&rec_b2, (uint16_t)sizeof(rec_b2));
        active = nv_store_df_pick_active_block_v2(&rec_a2, &rec_b2);
        if (active == NV_STORE_DF_BLOCK_A)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_A;
            s_df_seq = rec_a2.seq;
            s_fcnt_up = rec_a2.fcnt_up;
            s_fcnt_down = 1UL;
            s_last_alarm_id = rec_a2.last_alarm_id;
            s_lora_channel_idx = rec_a2.lora_channel_idx;
            memset(s_pan_id, 0, sizeof(s_pan_id));
            memset(s_seri_ed, 0, sizeof(s_seri_ed));
            s_fire_start_epoch_s = 0UL;
            nv_store_df_commit(1U);
            return;
        }
        else if (active == NV_STORE_DF_BLOCK_B)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_B;
            s_df_seq = rec_b2.seq;
            s_fcnt_up = rec_b2.fcnt_up;
            s_fcnt_down = 1UL;
            s_last_alarm_id = rec_b2.last_alarm_id;
            s_lora_channel_idx = rec_b2.lora_channel_idx;
            memset(s_pan_id, 0, sizeof(s_pan_id));
            memset(s_seri_ed, 0, sizeof(s_seri_ed));
            s_fire_start_epoch_s = 0UL;
            nv_store_df_commit(1U);
            return;
        }

        /* Fallback: read V1 and migrate to V2. */
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_A, (uint8_t *)&rec_a1, (uint16_t)sizeof(rec_a1));
        nv_store_df_read_block_raw(NV_STORE_DF_BLOCK_B, (uint8_t *)&rec_b1, (uint16_t)sizeof(rec_b1));
        active = nv_store_df_pick_active_block_v1(&rec_a1, &rec_b1);
        if (active == NV_STORE_DF_BLOCK_A)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_A;
            s_df_seq = rec_a1.seq;
            s_fcnt_up = rec_a1.fcnt_up;
            s_fcnt_down = 1UL;
            s_last_alarm_id = rec_a1.last_alarm_id;
            s_lora_channel_idx = 0U;
            memset(s_pan_id, 0, sizeof(s_pan_id));
            memset(s_seri_ed, 0, sizeof(s_seri_ed));
            s_fire_start_epoch_s = 0UL;
            nv_store_df_commit(1U);
            return;
        }
        else if (active == NV_STORE_DF_BLOCK_B)
        {
            s_df_active_block = NV_STORE_DF_BLOCK_B;
            s_df_seq = rec_b1.seq;
            s_fcnt_up = rec_b1.fcnt_up;
            s_fcnt_down = 1UL;
            s_last_alarm_id = rec_b1.last_alarm_id;
            s_lora_channel_idx = 0U;
            memset(s_pan_id, 0, sizeof(s_pan_id));
            memset(s_seri_ed, 0, sizeof(s_seri_ed));
            s_fire_start_epoch_s = 0UL;
            nv_store_df_commit(1U);
            return;
        }
    }

    /* Defaults (first boot or DF unavailable). */
    s_fcnt_up = 1UL;
    s_fcnt_down = 1UL;
    s_last_alarm_id = 0U;
    s_lora_channel_idx = 0U;
    memset(s_pan_id, 0, sizeof(s_pan_id));
    memset(s_seri_ed, 0, sizeof(s_seri_ed));
    s_fire_start_epoch_s = 0UL;
    s_lora_rssi_threshold_dbm = NV_STORE_CFG_RSSI_UNSET_DBM;
    s_heartbeat_period_s = NV_STORE_CFG_HB_UNSET_S;
    s_smoke_sensitivity = NV_STORE_CFG_SENS_UNSET;
    s_heat_sensitivity = NV_STORE_CFG_SENS_UNSET;

    nv_store_df_commit(1U);
}

void nv_store_factory_reset(void)
{
    if (s_df_ready == 0U)
    {
        (void)hal_dataflash_init();
        s_df_ready = 1U;
    }

    if (s_df_ready != 0U)
    {
        (void)hal_dataflash_erase_block(NV_STORE_DF_BLOCK_A);
        (void)hal_dataflash_erase_block(NV_STORE_DF_BLOCK_B);
    }

    s_df_active_block = NV_STORE_DF_BLOCK_A;
    s_df_seq = 0UL;

    s_fcnt_up = 1UL;
    s_fcnt_down = 1UL;
    s_last_alarm_id = 0U;
    s_lora_channel_idx = 0U;
    memset(s_pan_id, 0, sizeof(s_pan_id));
    memset(s_seri_ed, 0, sizeof(s_seri_ed));
    s_fire_start_epoch_s = 0UL;
    s_lora_rssi_threshold_dbm = NV_STORE_CFG_RSSI_UNSET_DBM;
    s_heartbeat_period_s = NV_STORE_CFG_HB_UNSET_S;
    s_smoke_sensitivity = NV_STORE_CFG_SENS_UNSET;
    s_heat_sensitivity = NV_STORE_CFG_SENS_UNSET;

    nv_store_df_commit(1U);
}

void nv_store_flush(void)
{
    nv_store_df_commit(1U);
}

uint32_t nv_store_get_fcnt_up(void)
{
    return s_fcnt_up;
}

void nv_store_set_fcnt_up(uint32_t v)
{
    s_fcnt_up = v;

    /* Checkpoint occasionally to protect Data Flash endurance. */
    nv_store_df_commit(0U);
}

uint32_t nv_store_get_fcnt_down(void)
{
    return s_fcnt_down;
}

void nv_store_set_fcnt_down(uint32_t v)
{
    s_fcnt_down = v;

    /* Checkpoint occasionally to protect Data Flash endurance. */
    nv_store_df_commit(0U);
}

uint16_t nv_store_get_last_alarm_id(void)
{
    return s_last_alarm_id;
}

void nv_store_set_last_alarm_id(uint16_t v)
{
    s_last_alarm_id = v;

    /* Infrequent value: commit immediately. */
    nv_store_df_commit(1U);
}

uint8_t nv_store_get_lora_channel_idx(void)
{
    return s_lora_channel_idx;
}

void nv_store_set_lora_channel_idx(uint8_t idx)
{
    s_lora_channel_idx = idx;

    /* Infrequent value: commit immediately. */
    nv_store_df_commit(1U);
}

void nv_store_get_pan_id(uint8_t out_pan_id[6])
{
    if (out_pan_id == NULL)
    {
        return;
    }
    memcpy(out_pan_id, s_pan_id, 6);
}

void nv_store_set_pan_id(const uint8_t pan_id[6])
{
    if (pan_id == NULL)
    {
        return;
    }

    memcpy(s_pan_id, pan_id, 6);

    /* Infrequent value: commit immediately. */
    nv_store_df_commit(1U);
}

void nv_store_get_seri_ed(uint8_t out_seri_ed[6])
{
    if (out_seri_ed == NULL)
    {
        return;
    }
    memcpy(out_seri_ed, s_seri_ed, 6);
}

void nv_store_set_seri_ed(const uint8_t seri_ed[6])
{
    if (seri_ed == NULL)
    {
        return;
    }

    memcpy(s_seri_ed, seri_ed, 6);

    /* Infrequent value: commit immediately. */
    nv_store_df_commit(1U);
}

uint32_t nv_store_get_fire_start_epoch_s(void)
{
    return s_fire_start_epoch_s;
}

void nv_store_set_fire_start_epoch_s(uint32_t epoch_s)
{
    s_fire_start_epoch_s = epoch_s;

    /* Infrequent value: commit immediately. */
    nv_store_df_commit(1U);
}

int16_t nv_store_get_lora_rssi_threshold_dbm(void)
{
    return s_lora_rssi_threshold_dbm;
}

void nv_store_set_lora_rssi_threshold_dbm(int16_t threshold_dbm)
{
    s_lora_rssi_threshold_dbm = threshold_dbm;

    /* Infrequent value: commit immediately. */
    nv_store_df_commit(1U);
}

uint16_t nv_store_get_heartbeat_period_s(void)
{
    return s_heartbeat_period_s;
}

void nv_store_set_heartbeat_period_s(uint16_t period_s)
{
    s_heartbeat_period_s = period_s;

    /* Infrequent value: commit immediately. */
    nv_store_df_commit(1U);
}

uint16_t nv_store_get_smoke_sensitivity(void)
{
    return s_smoke_sensitivity;
}

void nv_store_set_smoke_sensitivity(uint16_t v)
{
    s_smoke_sensitivity = v;

    /* Infrequent value: commit immediately. */
    nv_store_df_commit(1U);
}

uint16_t nv_store_get_heat_sensitivity(void)
{
    return s_heat_sensitivity;
}

void nv_store_set_heat_sensitivity(uint16_t v)
{
    s_heat_sensitivity = v;

    /* Infrequent value: commit immediately. */
    nv_store_df_commit(1U);
}
