#include "lib/myos.h"

int main() {
    char buf[256];
    puts("Echo program ready. Type something:\n");
    while (1) {
        puts("> ");
        uint32_t len = read(buf, 255);
        if (len > 0) {
            puts("You said: ");
            write(buf, len, STDOUT);
            puts("\n");
        }
    }
    return 0;
}

