#include "lib/myos.h"

int main() {
    puts("Opening /readme.txt...\n");
    int fd = open("/readme.txt");
    if (fd < 0) {
        puts("open failed!\n");
        exit();
    }
    puts("File size: ");
    print_num(fsize(fd));
    puts(" bytes\n");

    char buf[128];
    int n = read_file(fd, buf, 127);
    if (n > 0) {
        buf[n] = '\0';
        puts("Contents: ");
        puts(buf);
        puts("\n");
    } else {
        puts("read returned 0 bytes\n");
    }
    close(fd);
    puts("Done!\n");
    exit();
}

