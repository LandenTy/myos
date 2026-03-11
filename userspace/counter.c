#include "lib/myos.h"

int main() {
    puts("Counter starting...\n");
    for (uint32_t i = 1; i <= 5; i++) {
        puts("Count: ");
        print_num(i);
        puts("\n");
        sleep(1000);
    }
    puts("Counter done!\n");
    return 0;
}

