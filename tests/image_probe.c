#include "rf/image.h"
#include <stdlib.h>
int main(int argc, char **argv)
{
    rf_vpp archive;
    rf_vpp_entry entry;
    rf_image image;
    FILE *output;
    int result;
    if (argc != 5) return 2;
    result = rf_vpp_open(&archive, argv[1]);
    if (result) return 1;
    result = rf_vpp_find(&archive, argv[2], &entry);
    if (!result) result = rf_image_tga(&image, &archive, &entry, (uint32_t)strtoul(argv[4], NULL, 10));
    rf_vpp_close(&archive);
    if (result) { printf("%d\n", result); return 1; }
    output = fopen(argv[3], "wb");
    if (!output) { rf_image_close(&image); return 1; }
    result = fwrite(image.rgba, 1, image.bytes, output) != image.bytes;
    if (fclose(output)) result = 1;
    printf("%u %u %u\n", image.width, image.height, image.bytes);
    rf_image_close(&image);
    return result;
}
