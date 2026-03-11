#include "lib/myos.h"

int main() {
    puts("Hello from a real ELF program!\n");
    puts("My PID is: ");
    print_num(getpid());
    puts("\n");
    puts("Uptime: ");
    print_num(uptime());
    puts(" ms\n");
    return 0;
}

