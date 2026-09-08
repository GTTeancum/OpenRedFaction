#include "rf/model_file.h"
int main(int argc, char **argv)
{
    rf_vpp archive;
    rf_model_file model;
    uint32_t i;
    int result;
    if (argc != 3 || rf_vpp_open(&archive, argv[1])) return 2;
    result = rf_model_file_open(&model, &archive, argv[2]);
    if (!result) for (i = 0; i < model.section_count; ++i)
        printf("%u %u %u\n", model.sections[i].type, model.sections[i].offset, model.sections[i].size);
    rf_vpp_close(&archive);
    return result ? 3 : 0;
}
