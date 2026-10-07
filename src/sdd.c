/*
 * Pico-emu Software-Defined Device (SDD) Framework
 *
 * Registry and lifecycle management for pluggable virtual peripherals.
 * Devices register their bus callbacks (I2C/SPI) and are automatically
 * attached to the appropriate peripheral bus.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdd.h"
#include "i2c.h"
#include "spi.h"
#include "vnet.h"

sdd_registry_t sdd_registry;

void sdd_init(void) {
    /* Repeat sdd_add starts from a clean slate: free prior contexts,
     * then detach their bus callbacks so no stale I2C/SPI entry keeps
     * pointing at a freed ctx (and keeps ACKing). Single-batch attach
     * keeps working: attach follows init, same as before. */
    sdd_cleanup();
    i2c_detach_all_devices();
    spi_detach_all_devices();
    memset(&sdd_registry, 0, sizeof(sdd_registry));
    jsmirror_reset();
    spimirror_reset();
}

void sdd_cleanup(void) {
    for (int i = 0; i < sdd_registry.count; i++) {
        sdd_device_t *dev = &sdd_registry.devices[i];
        if (dev->cleanup) {
            dev->cleanup(dev->ctx);
        }
    }
    sdd_registry.count = 0;
}

int sdd_register(sdd_device_t *dev) {
    if (sdd_registry.count >= SDD_MAX_DEVICES) {
        fprintf(stderr, "[SDD] Maximum devices (%d) reached\n", SDD_MAX_DEVICES);
        return -1;
    }

    int idx = sdd_registry.count++;
    memcpy(&sdd_registry.devices[idx], dev, sizeof(sdd_device_t));
    sdd_device_t *d = &sdd_registry.devices[idx];

    /* Auto-attach to I2C bus */
    if (d->i2c_addr >= 0 && d->i2c_bus >= 0 && d->i2c_bus <= 1) {
        i2c_attach_device(d->i2c_bus, (uint8_t)d->i2c_addr,
                          d->i2c_write, d->i2c_read,
                          d->i2c_start, d->i2c_stop, d->ctx);
        fprintf(stderr, "[SDD] '%s' attached to I2C%d at 0x%02X\n",
                d->name, d->i2c_bus, d->i2c_addr);
    }

    /* Auto-attach to SPI bus */
    if (d->spi_bus >= 0 && d->spi_bus <= 1 && d->spi_xfer) {
        spi_attach_device(d->spi_bus, d->spi_xfer, d->spi_cs, d->ctx);
        fprintf(stderr, "[SDD] '%s' attached to SPI%d\n", d->name, d->spi_bus);
    }

    /* Auto-attach to vnet */
    if (d->net_rx) {
        uint8_t zero_mac[6] = {0};
        const uint8_t *mac = memcmp(d->mac, zero_mac, 6) != 0 ? d->mac : NULL;
        vnet_register_port(d->name, VNET_PORT_SDD, mac, d->net_rx, d->ctx);
    }

    fprintf(stderr, "[SDD] Device '%s' registered (index %d)\n", d->name, idx);
    return idx;
}

void sdd_tick_all(uint32_t elapsed_us) {
    for (int i = 0; i < sdd_registry.count; i++) {
        sdd_device_t *dev = &sdd_registry.devices[i];
        if (dev->tick) {
            dev->tick(dev->ctx, elapsed_us);
        }
    }
}

/* ========================================================================
 * Argument parsing for -sdd flag
 *
 * Format: type[:key=val,key=val,...]
 * ======================================================================== */

int sdd_create_from_arg(const char *arg) {
    char buf[256];
    strncpy(buf, arg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Split type from options */
    char *type = buf;
    char *opts = strchr(buf, ':');
    if (opts) {
        *opts = '\0';
        opts++;
    }

    if (strcmp(type, "thermometer") == 0 || strcmp(type, "thermo") == 0) {
        float temp = 25.0f;
        int i2c_bus = 0;
        int i2c_addr = 0x48;

        /* Parse options */
        if (opts) {
            char *saveptr = NULL;
            char *token = strtok_r(opts, ",", &saveptr);
            while (token) {
                if (strncmp(token, "temp=", 5) == 0) {
                    temp = (float)atof(token + 5);
                } else if (strncmp(token, "i2c=", 4) == 0) {
                    i2c_bus = atoi(token + 4);
                } else if (strncmp(token, "addr=", 5) == 0) {
                    i2c_addr = (int)strtol(token + 5, NULL, 0);
                }
                token = strtok_r(NULL, ",", &saveptr);
            }
        }

        return sdd_create_thermometer(temp, i2c_bus, i2c_addr);
    }

    if (strcmp(type, "eeprom") == 0) {
        int i2c_bus = 0;
        int i2c_addr = 0x50;
        const char *file = NULL;

        if (opts) {
            char *saveptr = NULL;
            char *token = strtok_r(opts, ",", &saveptr);
            while (token) {
                if (strncmp(token, "i2c=", 4) == 0) {
                    i2c_bus = atoi(token + 4);
                } else if (strncmp(token, "addr=", 5) == 0) {
                    i2c_addr = (int)strtol(token + 5, NULL, 0);
                } else if (strncmp(token, "file=", 5) == 0) {
                    file = token + 5;
                }
                token = strtok_r(NULL, ",", &saveptr);
            }
        }

        return sdd_create_eeprom(i2c_bus, i2c_addr, file);
    }

    /* JS-mirror slave(s). One sdd_add call attaches
     * all of them (the registry resets per call), sharing one ring:
     *   jsmirror:i2c=0,addr=0x3c
     *   jsmirror:i2c=0,addr=0x3c,addr=0x27
     */
    if (strcmp(type, "jsmirror") == 0) {
        int i2c_bus = 0;
        int addrs[8];
        int naddrs = 0;

        if (opts) {
            char *saveptr = NULL;
            char *token = strtok_r(opts, ",", &saveptr);
            while (token) {
                if (strncmp(token, "i2c=", 4) == 0) {
                    i2c_bus = atoi(token + 4);
                } else if (strncmp(token, "addr", 4) == 0) {
                    const char *eq = strchr(token, '=');
                    if (eq && naddrs < 8) {
                        addrs[naddrs++] = (int)strtol(eq + 1, NULL, 0) & 0x7F;
                    }
                }
                token = strtok_r(NULL, ",", &saveptr);
            }
        }

        if (naddrs == 0) {
            fprintf(stderr, "[SDD] jsmirror needs addr=0x.. (e.g. jsmirror:i2c=0,addr=0x3c)\n");
            return -1;
        }
        return sdd_create_jsmirror(i2c_bus, addrs, naddrs);
    }

    /* SPI-mirror slave (MOSI observe + MISO inject, replaces any
     * sdcard/emmc on the bus): spimirror:spi=0 */
    if (strcmp(type, "spimirror") == 0) {
        int spi_bus = 0;

        if (opts) {
            char *saveptr = NULL;
            char *token = strtok_r(opts, ",", &saveptr);
            while (token) {
                if (strncmp(token, "spi=", 4) == 0) {
                    spi_bus = atoi(token + 4);
                }
                token = strtok_r(NULL, ",", &saveptr);
            }
        }

        return sdd_create_spimirror(spi_bus);
    }

    fprintf(stderr, "[SDD] Unknown device type: '%s'\n", type);
    fprintf(stderr, "[SDD] Available types: thermometer, eeprom, jsmirror, spimirror\n");
    return -1;
}
