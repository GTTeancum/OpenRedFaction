#include "rf/checksum.h"
#include <stdio.h>
#include <string.h>

static int hex(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
int main(void)
{
    char line[4096], bytes[2048];
    while (fgets(line, sizeof(line), stdin)) {
        size_t length = strcspn(line, "\r\n"), i;
        if (length == 4 && memcmp(line, "NULL", 4) == 0) {
            printf("%08x\n", rf_filename_checksum(NULL));
            continue;
        }
        if (length % 2 || length / 2 >= sizeof(bytes)) return 2;
        for (i = 0; i < length; i += 2) {
            int high = hex(line[i]), low = hex(line[i + 1]);
            if (high < 0 || low < 0) return 2;
            bytes[i / 2] = (char)(high * 16 + low);
        }
        bytes[length / 2] = 0;
        printf("%08x\n", rf_filename_checksum(bytes));
    }
    return 0;
}
