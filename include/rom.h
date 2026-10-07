#ifndef ROM_H
#define ROM_H

#include <stdint.h>

/* ROM is mapped at 0x00000000, 16KB (matches real RP2040 bootrom size) */
#define ROM_BASE    0x00000000
#define ROM_SIZE    0x4000

/* ROM layout offsets */
#define ROM_MAGIC_OFFSET    0x10    /* 'M', 'u', version */
#define ROM_FUNC_TABLE_PTR  0x14    /* 16-bit pointer to function table */
#define ROM_DATA_TABLE_PTR  0x16    /* 16-bit pointer to data table */
#define ROM_LOOKUP_FN_PTR   0x18    /* 16-bit pointer to lookup function */

/* ROM table code macro (matches SDK) */
#define ROM_TABLE_CODE(c1, c2) ((c1) | ((c2) << 8))

/* Known ROM function codes */
#define ROM_FUNC_MEMCPY     ROM_TABLE_CODE('M', 'C')
#define ROM_FUNC_MEMCPY44   ROM_TABLE_CODE('C', '4')
#define ROM_FUNC_MEMSET     ROM_TABLE_CODE('M', 'S')
#define ROM_FUNC_MEMSET4    ROM_TABLE_CODE('S', '4')
#define ROM_FUNC_POPCOUNT32 ROM_TABLE_CODE('P', '3')
#define ROM_FUNC_CLZ32      ROM_TABLE_CODE('L', '3')
#define ROM_FUNC_CTZ32      ROM_TABLE_CODE('T', '3')
#define ROM_FUNC_REVERSE32  ROM_TABLE_CODE('R', '3')

/* Flash ROM function codes */
#define ROM_FUNC_CONNECT_INTERNAL_FLASH ROM_TABLE_CODE('I', 'F')
#define ROM_FUNC_FLASH_EXIT_XIP         ROM_TABLE_CODE('E', 'X')
#define ROM_FUNC_FLASH_RANGE_ERASE      ROM_TABLE_CODE('R', 'E')
#define ROM_FUNC_FLASH_RANGE_PROGRAM    ROM_TABLE_CODE('R', 'P')
#define ROM_FUNC_FLASH_FLUSH_CACHE      ROM_TABLE_CODE('F', 'C')
#define ROM_FUNC_FLASH_ENTER_CMD_XIP    ROM_TABLE_CODE('C', 'X')

/* RP2350-only ROM function codes (RP2350 datasheet § bootrom) */
#define ROM_FUNC_GET_SYS_INFO           ROM_TABLE_CODE('G', 'S')
#define ROM_FUNC_REBOOT                 ROM_TABLE_CODE('R', 'B')
/* bootrom_state_reset(flags): resets bootrom global/per-core state
 * (permissions, callbacks). No secure state exists in emulation → no-op. */
#define ROM_FUNC_BOOTROM_STATE_RESET    ROM_TABLE_CODE('S', 'R')

/* ROM data table codes */
#define ROM_DATA_SOFT_FLOAT  ROM_TABLE_CODE('S', 'F')
#define ROM_DATA_SOFT_DOUBLE ROM_TABLE_CODE('S', 'D')

/* ROM float/double function stub address ranges.
 * Silicon soft_float/soft_double tables hold 32-bit routine addresses
 * (the SDK copies them word-wise into sf/sd_table), so each stub slot
 * is 4 bytes: BX LR at BASE+i*4, index = (pc-BASE)/4. Indices follow
 * the silicon V1 order (sf_table.h), NOT the old packed order. */
#define ROM_FLOAT_FUNC_BASE   0x0500
#define ROM_FLOAT_FUNC_COUNT  21
#define ROM_DOUBLE_FUNC_BASE  0x0560
#define ROM_DOUBLE_FUNC_COUNT 21

/* Float/double function indices (silicon V1 order: FADD..FDIV,
 * FCMP_FAST/_FLAGS, FSQRT, conversions, trig, FSINCOS, FEXP, FLN).
 * FCMP_FAST/_FLAGS and FSINCOS have BX-LR stubs only (no callers in
 * the SDK wrappers / rare sincos); everything else is implemented
 * natively by rom_intercept. */
#define ROM_FLOAT_FADD        0
#define ROM_FLOAT_FSUB        1
#define ROM_FLOAT_FMUL        2
#define ROM_FLOAT_FDIV        3
#define ROM_FLOAT_FCMP_FAST   4
#define ROM_FLOAT_FCMP_FAST_FLAGS 5
#define ROM_FLOAT_FSQRT       6
#define ROM_FLOAT_FLOAT2INT   7
#define ROM_FLOAT_FLOAT2FIX   8
#define ROM_FLOAT_FLOAT2UINT  9
#define ROM_FLOAT_FLOAT2UFIX  10
#define ROM_FLOAT_INT2FLOAT   11
#define ROM_FLOAT_FIX2FLOAT   12
#define ROM_FLOAT_UINT2FLOAT  13
#define ROM_FLOAT_UFIX2FLOAT  14
#define ROM_FLOAT_FCOS        15
#define ROM_FLOAT_FSIN        16
#define ROM_FLOAT_FTAN        17
#define ROM_FLOAT_FSINCOS     18
#define ROM_FLOAT_FEXP        19
#define ROM_FLOAT_FLN         20

/* ROM flash function stub addresses */
#define ROM_FLASH_RANGE_ERASE_ADDR   0x03B8
#define ROM_FLASH_RANGE_PROGRAM_ADDR 0x03BC

/* ROM image buffer */
extern uint8_t rom_image[ROM_SIZE];

/* Initialize ROM image with function table and Thumb code stubs */
void rom_init(void);

/* Read from ROM (used by membus) */
uint32_t rom_read32(uint32_t addr);
uint16_t rom_read16(uint32_t addr);
uint8_t  rom_read8(uint32_t addr);

/* ROM function interception: returns 1 if PC was intercepted */
int rom_intercept(uint32_t pc);

/* Patch ROM for RP2350 ARM/M33 mode (call after rom_init in M33 path) */
void rom_patch_rp2350_arm(void);

#endif /* ROM_H */
