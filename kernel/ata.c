#include "ata.h"
#include "serial.h"
#include "../include/io.h"
#include "../include/vga.h"
#include <stdint.h>

static ata_drive_t primary_master;

static void ata_400ns_delay(uint16_t base) {
    inb(base + ATA_REG_STATUS);
    inb(base + ATA_REG_STATUS);
    inb(base + ATA_REG_STATUS);
    inb(base + ATA_REG_STATUS);
}

static int ata_poll(uint16_t base) {
    ata_400ns_delay(base);

    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(base + ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) return -1;
        if (status & ATA_STATUS_DF)  return -2;
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ))
            return 0;
    }
    return -3;
}

static int ata_identify(ata_drive_t *drive) {
    outb(drive->base + ATA_REG_DRIVE, drive->slave ? 0xB0 : 0xA0);
    ata_400ns_delay(drive->base);

    outb(drive->base + ATA_REG_SECCOUNT, 0);
    outb(drive->base + ATA_REG_LBA0,     0);
    outb(drive->base + ATA_REG_LBA1,     0);
    outb(drive->base + ATA_REG_LBA2,     0);
    outb(drive->base + ATA_REG_COMMAND,  ATA_CMD_IDENTIFY);

    uint8_t status = inb(drive->base + ATA_REG_STATUS);
    if (status == 0) return -1;

    for (int i = 0; i < 100000; i++) {
        status = inb(drive->base + ATA_REG_STATUS);
        if (!(status & ATA_STATUS_BSY)) break;
    }

    if (inb(drive->base + ATA_REG_LBA1) != 0 ||
        inb(drive->base + ATA_REG_LBA2) != 0)
        return -1;

    for (int i = 0; i < 100000; i++) {
        status = inb(drive->base + ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) return -1;
        if (status & ATA_STATUS_DRQ) break;
    }

    uint16_t buf[256];
    for (int i = 0; i < 256; i++)
        buf[i] = inw(drive->base + ATA_REG_DATA);

    for (int i = 0; i < 20; i++) {
        drive->model[i*2]   = (buf[27+i] >> 8) & 0xFF;
        drive->model[i*2+1] = (buf[27+i])      & 0xFF;
    }
    drive->model[40] = '\0';

    int last = 39;
    while (last > 0 && drive->model[last] == ' ')
        drive->model[last--] = '\0';

    drive->sectors = ((uint32_t)buf[61] << 16) | buf[60];
    drive->exists  = 1;
    return 0;
}

int ata_init() {
    primary_master.base  = ATA_PRIMARY_BASE;
    primary_master.ctrl  = ATA_PRIMARY_CTRL;
    primary_master.slave = 0;
    primary_master.exists = 0;

    outb(primary_master.ctrl, 0);

    if (ata_identify(&primary_master) == 0) {
        serial_puts("[ATA] Primary master: ");
        serial_puts(primary_master.model);
        serial_puts(" sectors=");
        serial_print_num(primary_master.sectors);
        serial_puts("\n");
        return 0;
    }

    serial_puts("[ATA] No primary master drive found\n");
    return -1;
}

int ata_read_sectors(uint32_t lba, uint8_t count, void *buf) {
    if (!primary_master.exists) return -1;

    uint16_t base = primary_master.base;

    outb(base + ATA_REG_DRIVE,
         0xE0 | ((lba >> 24) & 0x0F));
    outb(base + ATA_REG_FEATURES, 0x00);
    outb(base + ATA_REG_SECCOUNT, count);
    outb(base + ATA_REG_LBA0,     (lba)       & 0xFF);
    outb(base + ATA_REG_LBA1,     (lba >> 8)  & 0xFF);
    outb(base + ATA_REG_LBA2,     (lba >> 16) & 0xFF);
    outb(base + ATA_REG_COMMAND,  ATA_CMD_READ_PIO);

    uint16_t *ptr = (uint16_t*)buf;
    for (int s = 0; s < count; s++) {
        if (ata_poll(base) != 0) return -1;
        for (int i = 0; i < 256; i++)
            ptr[s * 256 + i] = inw(base + ATA_REG_DATA);
    }
    return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const void *buf) {
    if (!primary_master.exists) return -1;

    uint16_t base = primary_master.base;

    outb(base + ATA_REG_DRIVE,
         0xE0 | ((lba >> 24) & 0x0F));
    outb(base + ATA_REG_FEATURES, 0x00);
    outb(base + ATA_REG_SECCOUNT, count);
    outb(base + ATA_REG_LBA0,     (lba)       & 0xFF);
    outb(base + ATA_REG_LBA1,     (lba >> 8)  & 0xFF);
    outb(base + ATA_REG_LBA2,     (lba >> 16) & 0xFF);
    outb(base + ATA_REG_COMMAND,  ATA_CMD_WRITE_PIO);

    const uint16_t *ptr = (const uint16_t*)buf;
    for (int s = 0; s < count; s++) {
        if (ata_poll(base) != 0) return -1;
        for (int i = 0; i < 256; i++)
            outw(base + ATA_REG_DATA, ptr[s * 256 + i]);
        outb(base + ATA_REG_COMMAND, 0xE7);
        ata_400ns_delay(base);
    }
    return 0;
}

void ata_print_info() {
    if (!primary_master.exists) {
        vga_puts(" No ATA drives found\n");
        return;
    }
    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts(" -- ATA Drive Info --\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts(" Model  : "); vga_puts(primary_master.model); vga_putchar('\n');
    vga_puts(" Sectors: ");
    char buf[12]; int i = 11; buf[i] = '\0';
    uint32_t n = primary_master.sectors;
    if (!n) { vga_putchar('0'); }
    else { while (n && i > 0) { buf[--i] = '0'+(n%10); n/=10; } vga_puts(&buf[i]); }
    vga_putchar('\n');
    vga_puts(" Size   : ");
    n = primary_master.sectors / 2048;
    i = 11; buf[i] = '\0';
    if (!n) { vga_putchar('0'); }
    else { while (n && i > 0) { buf[--i] = '0'+(n%10); n/=10; } vga_puts(&buf[i]); }
    vga_puts(" MB\n");
}

