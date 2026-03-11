#include "keyboard.h"
#include "irq.h"
#include "../include/io.h"
#include <stdint.h>

#define KB_BUF_SIZE 256

static const char sc_ascii_low[] = {
    0, 0,
    '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\',
    'z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' '
};

static const char sc_ascii_high[] = {
    0, 0,
    '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,
    'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|',
    'Z','X','C','V','B','N','M','<','>','?',
    0,'*',0,' '
};

static volatile char kb_buf[KB_BUF_SIZE];
static volatile int  kb_head    = 0;
static volatile int  kb_tail    = 0;
static volatile int  shift_held = 0;
static volatile int  caps_lock  = 0;

static void kb_push(char c) {
    int next = (kb_head + 1) % KB_BUF_SIZE;
    if (next != kb_tail) {
        kb_buf[kb_head] = c;
        kb_head = next;
    }
}

static void keyboard_irq_handler(struct regs *r) {
    (void)r;
    if (!(inb(0x64) & 0x1)) return;
    uint8_t sc = inb(0x60);

    if (sc == 0x2A || sc == 0x36) { shift_held = 1; return; }
    if (sc == 0xAA || sc == 0xB6) { shift_held = 0; return; }
    if (sc == 0x3A) { caps_lock = !caps_lock; return; }
    if (sc & 0x80) return;

    if (sc < sizeof(sc_ascii_low)) {
        int use_upper = shift_held ^ caps_lock;
        char c = use_upper ? sc_ascii_high[sc] : sc_ascii_low[sc];
        if (c) kb_push(c);
    }
}

void keyboard_init() {
    while (inb(0x64) & 0x1) inb(0x60);
    irq_install_handler(1, keyboard_irq_handler);
}

char keyboard_read() {
    // Sleep with hlt until a key arrives — no CPU burning
    while (kb_head == kb_tail) {
        __asm__ volatile ("sti; hlt");
    }
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return c;
}

int keyboard_available() {
    return kb_head != kb_tail;
}

