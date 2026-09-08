#include "rf/model.h"
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(int argc,char **argv)
{
    struct { uint32_t counts[3]; char names[3][16][32]; char query[32]; } input;
    struct { int32_t status, index; } output;
    rf_model_name names[3][16];
    rf_model_name_group groups[3];
    uint32_t g, n;
    _Static_assert(sizeof(input) == 1580, "Probe wire layout");
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
    if (argc==2 && !strcmp(argv[1],"--material-copy")) {
        struct { int32_t kind; rf_model_material_record source,destination; } data;
        while (fread(&data,sizeof(data),1,stdin)==1) {
            uint32_t counts[3]={99,99,99}; int32_t status;
            status=rf_model_material_prepare_copy(&data.destination,&data.source,data.kind,counts);
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&data.destination,200,1,stdout)!=1 || fwrite(counts,12,1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc==2 && !strcmp(argv[1],"--material-init")) {
        rf_model_material_record material;
        while (fread(&material,sizeof(material),1,stdin)==1) {
            if (rf_model_material_initialize(&material)!=RF_OK) return 2;
            if (fwrite(&material,sizeof(material),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc==2 && !strcmp(argv[1],"--materials")) {
        struct { int32_t kind,lods,static_count,mesh_count,direct_count,counts[8]; uint32_t capacity; } data;
        _Static_assert(sizeof(data)==56,"Material count fixture layout");
        while (fread(&data,sizeof(data),1,stdin)==1) {
            int32_t result[2]={0,-99};
            if (data.capacity>8) return 2;
            result[0]=rf_model_material_count(data.kind,data.lods,data.static_count,data.mesh_count,
                data.counts,data.capacity,data.direct_count,&result[1]);
            if (fwrite(result,sizeof(result),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    while (fread(&input, sizeof(input), 1, stdin) == 1) {
        for (g = 0; g < 3; ++g) {
            if (input.counts[g] > 16) return 2;
            groups[g].names = names[g]; groups[g].count = input.counts[g];
            for (n = 0; n < input.counts[g]; ++n) {
                if (!memchr(input.names[g][n], 0, 32)) return 2;
                names[g][n].data = input.names[g][n];
                names[g][n].length = strlen(input.names[g][n]);
            }
        }
        if (!memchr(input.query, 0, 32)) return 2;
        {
            rf_model_name query = {input.query, strlen(input.query)};
            output.index = -1;
            output.status = rf_model_find_tag(groups, query, &output.index);
        }
        if (fwrite(&output, sizeof(output), 1, stdout) != 1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
