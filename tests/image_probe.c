#include "rf/image.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(int argc, char **argv)
{
    rf_vpp archive;
    rf_vpp_entry entry;
    rf_image image;
    FILE *output;
    int result;
    if(argc==2 && !strcmp(argv[1],"--sample-owned")) {
        struct {uint32_t width,height,format,packed;float u,v;unsigned char pixels[256];} in;
        struct {int32_t status;uint32_t color;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_image image={in.width,in.height,in.width*in.height*(in.packed?2u:4u),in.format,in.pixels};
            out.color=0xa5a5a5a5;out.status=image.bytes>256?RF_RANGE:rf_image_sample_owned(&image,in.u,in.v,&out.color);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sample-locked")) {
        struct {uint32_t width,height,pitch,format,bytes;float u,v;unsigned char pixels[512];} in;
        struct {int32_t status;uint32_t color;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_image_sample_surface surface={in.width,in.height,in.pitch,in.format,in.bytes,in.pixels};
            out.color=0xa5a5a5a5;out.status=in.bytes>512?RF_RANGE:rf_image_sample_locked(&surface,in.u,in.v,&out.color);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if (argc==3 && !strcmp(argv[1],"--format")) {
        uint32_t value=(uint32_t)strtoul(argv[2],NULL,10);
        printf("%u %d\n",rf_image_tga_format(value),rf_image_format_has_alpha(value));return 0;
    }
    if (argc != 5 && argc != 6) return 2;
    result = rf_vpp_open(&archive, argv[1]);
    if (result) return 1;
    result = rf_vpp_find(&archive, argv[2], &entry);
    if (!result) result = argc==6?rf_image_vbm_frame(&image,&archive,&entry,(uint32_t)strtoul(argv[5],NULL,10),(uint32_t)strtoul(argv[4],NULL,10),NULL,NULL):rf_image_open(&image, &archive, &entry, (uint32_t)strtoul(argv[4], NULL, 10));
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
