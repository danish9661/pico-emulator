#ifndef I2C_H
#define I2C_H

#include <stdint.h>

/* ========================================================================
 * RP2040 DW_apb_i2c Controller
 *
 * Two I2C instances with 16-deep TX/RX FIFOs and device callback support.
 * Attach external device models via i2c_attach_device().
 *
 * DW_apb_i2c DATA_CMD register:
 *   bit 8 = CMD: 0=write, 1=read
 *   bit 9 = STOP
 *   bit 10 = RESTART
 *   bits [7:0] = data byte (for write)
 * ======================================================================== */

/* RP2040 has two DW_apb_i2c controllers */
#define I2C0_BASE       0x40044000
#define I2C1_BASE       0x40048000
#define I2C_BLOCK_SIZE  0x1000

/* DW_apb_i2c register offsets */
#define I2C_CON             0x000
#define I2C_TAR             0x004
#define I2C_SAR             0x008
#define I2C_DATA_CMD        0x010
#define I2C_SS_SCL_HCNT     0x014
#define I2C_SS_SCL_LCNT     0x018
#define I2C_FS_SCL_HCNT     0x01C
#define I2C_FS_SCL_LCNT     0x020
#define I2C_INTR_STAT       0x02C
#define I2C_INTR_MASK       0x030
#define I2C_RAW_INTR_STAT   0x034
#define I2C_RX_TL           0x038
#define I2C_TX_TL           0x03C
#define I2C_CLR_INTR        0x040
#define I2C_CLR_RX_UNDER    0x044
#define I2C_CLR_RX_OVER     0x048
#define I2C_CLR_TX_OVER     0x04C
#define I2C_CLR_TX_ABRT     0x054
#define I2C_CLR_STOP_DET    0x060
#define I2C_CLR_START_DET   0x064
#define I2C_ENABLE          0x06C
#define I2C_STATUS          0x070
#define I2C_TXFLR           0x074
#define I2C_RXFLR           0x078
#define I2C_SDA_HOLD        0x07C
#define I2C_TX_ABRT_SOURCE  0x080
#define I2C_DMA_CR          0x088
#define I2C_DMA_TDLR        0x08C
#define I2C_DMA_RDLR        0x090
#define I2C_FS_SPKLEN       0x0A0
#define I2C_CLR_RESTART_DET 0x0A8
#define I2C_COMP_PARAM_1    0x0F4
#define I2C_COMP_VERSION    0x0F8
#define I2C_COMP_TYPE       0x0FC

/* Status register bits */
#define I2C_STATUS_ACTIVITY (1u << 0)
#define I2C_STATUS_TFNF     (1u << 1)
#define I2C_STATUS_TFE      (1u << 2)
#define I2C_STATUS_RFNE     (1u << 3)
#define I2C_STATUS_RFF      (1u << 4)
#define I2C_STATUS_MST_ACT  (1u << 5)

/* Raw interrupt bits */
#define I2C_INT_RX_UNDER    (1u << 0)
#define I2C_INT_RX_OVER     (1u << 1)
#define I2C_INT_RX_FULL     (1u << 2)
#define I2C_INT_TX_OVER     (1u << 3)
#define I2C_INT_TX_EMPTY    (1u << 4)
#define I2C_INT_TX_ABRT     (1u << 6)
#define I2C_INT_STOP_DET    (1u << 9)
#define I2C_INT_START_DET   (1u << 10)

/* TX_ABRT_SOURCE bits (Synopsys DW_apb_i2c) */
#define I2C_TX_ABRT_7B_ADDR_NOACK  (1u << 0)  /* address byte NACKed */
#define I2C_TX_ABRT_TXDATA_NOACK   (1u << 3)  /* data byte NACKed by slave */

/* DATA_CMD bits */
#define I2C_DATA_CMD_READ   (1u << 8)
#define I2C_DATA_CMD_STOP   (1u << 9)
#define I2C_DATA_CMD_RESTART (1u << 10)

#define I2C_FIFO_SIZE   16  /* DW_apb_i2c has 16-deep FIFOs */

/* I2C device callback interface */
typedef int     (*i2c_device_write_fn)(void *ctx, uint8_t data);
typedef uint8_t (*i2c_device_read_fn)(void *ctx);
typedef void    (*i2c_device_event_fn)(void *ctx);

#define I2C_MAX_DEVICES  8

typedef struct {
    uint8_t addr;                   /* 7-bit I2C address */
    i2c_device_write_fn write_fn;
    i2c_device_read_fn  read_fn;
    i2c_device_event_fn start_fn;   /* Optional */
    i2c_device_event_fn stop_fn;    /* Optional */
    void *ctx;
} i2c_device_entry_t;

/* Per-I2C state */
typedef struct {
    uint32_t con;
    uint32_t tar;
    uint32_t sar;
    uint32_t ss_scl_hcnt;
    uint32_t ss_scl_lcnt;
    uint32_t fs_scl_hcnt;
    uint32_t fs_scl_lcnt;
    uint32_t intr_mask;
    uint32_t raw_intr_stat;
    uint32_t rx_tl;
    uint32_t tx_tl;
    uint32_t enable;
    uint32_t sda_hold;
    uint32_t dma_cr;
    uint32_t dma_tdlr;
    uint32_t dma_rdlr;
    uint32_t fs_spklen;

    /* RX FIFO */
    uint8_t rx_fifo[I2C_FIFO_SIZE];
    int rx_head, rx_tail, rx_count;

    /* TX abort: sticky until CLR_TX_ABRT (clear-on-read). Set when a
     * transfer targets an address with no attached device (ADDR_NOACK)
     * or a slave NACKs a data byte (TXDATA_NOACK, write_fn != 0). */
    uint32_t tx_abrt_source;

    /* Attached devices */
    i2c_device_entry_t devices[I2C_MAX_DEVICES];
    int device_count;
} i2c_state_t;

extern i2c_state_t i2c_state[2];

void i2c_init(void);
uint32_t i2c_read32(int i2c_num, uint32_t offset);
void i2c_write32(int i2c_num, uint32_t offset, uint32_t val);
int i2c_match(uint32_t addr);

/* Attach a device to an I2C bus at the given 7-bit address */
int i2c_attach_device(int i2c_num, uint8_t addr,
                      i2c_device_write_fn write_fn,
                      i2c_device_read_fn read_fn,
                      i2c_device_event_fn start_fn,
                      i2c_device_event_fn stop_fn,
                      void *ctx);

/* Drop every attached slave on both buses (SDD re-provisioning).
 * Hardware RESETS preserve attachments (see i2c_reset_instance);
 * this is only for sdd_init() re-runs, where the registry (and its
 * device contexts) is discarded — otherwise stale entries keep
 * pointing at freed contexts and keep ACKing. */
void i2c_detach_all_devices(void);

/* Bit-bang (GPIO) slave bridge: does any attached device (either bus)
 * ACK this 7-bit address? Used by the GPIO-level I2C observer so
 * soft-I2C scans (MicroPython machine.I2C.scan) see real slaves. */
int i2c_probe_ack(uint8_t addr);

/* Bit-bang read path: next byte from the first device at addr
 * (open-bus 0xFF when none — same as the DW read path default). */
uint8_t i2c_slave_read_byte(uint8_t addr);

/* Bit-bang write path: deliver one data byte to the device at addr.
 * Returns 0 on ACK, nonzero on NACK (no device). */
int i2c_slave_write_byte(uint8_t addr, uint8_t data);

/* True when at least one I2C device is attached on either bus.
 * Gates the GPIO observer + open-drain release restore so a bare
 * board (no sdd_add) stays bit-identical. */
int i2c_any_attached(void);

#endif /* I2C_H */
