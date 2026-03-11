#pragma once
#include <stdint.h>

#define ATA_PRIMARY_BASE    0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_BASE  0x170
#define ATA_SECONDARY_CTRL  0x376

#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA0        0x03
#define ATA_REG_LBA1        0x04
#define ATA_REG_LBA2        0x05
#define ATA_REG_DRIVE       0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07

#define ATA_STATUS_ERR      0x01
#define ATA_STATUS_DRQ      0x08
#define ATA_STATUS_SRV      0x10
#define ATA_STATUS_DF       0x20
#define ATA_STATUS_RDY      0x40
#define ATA_STATUS_BSY      0x80

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_IDENTIFY    0xEC

typedef struct {
    uint16_t base;
    uint16_t ctrl;
    uint8_t  slave;
    uint8_t  exists;
    char     model[41];
    uint32_t sectors;
} ata_drive_t;

int      ata_init();
int      ata_read_sectors(uint32_t lba, uint8_t count, void *buf);
int      ata_write_sectors(uint32_t lba, uint8_t count, const void *buf);
void     ata_print_info();

