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
    if(argc==2 && !strcmp(argv[1],"--prepare-skinning")) {
        struct { float stored[4][12],pose[4][12],prepared[4][12];uint16_t stamps[4],generation,pad; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            if(rf_model_prepare_skinning(data.stored,data.pose,4,data.generation,data.prepared,data.stamps,4)!=RF_OK)return 2;
            if(fwrite(data.prepared,192,1,stdout)!=1 || fwrite(data.stamps,8,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--lighting-setup")) {
        struct { rf_model_lighting_input input;rf_model_local_light lights[8];float colors[8][3]; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            rf_model_lighting out;
            if(rf_model_lighting_setup(&data.input,data.lights,data.colors,8,&out)!=RF_OK)return 2;
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--local-light-direction")) {
        struct { float delta[3];uint32_t flags;float rotation[9]; } data;float out[3];
        while(fread(&data,sizeof(data),1,stdin)==1) {
            if(rf_model_local_light_direction(data.delta,data.flags,data.rotation,out)!=RF_OK)return 2;
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--local-light-color")) {
        float data[5],out[3];
        while(fread(data,sizeof(data),1,stdin)==1) {
            if(rf_model_local_light_color(data[0],data[1],data+2,out)!=RF_OK)return 2;
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--choose-light")) {
        struct { float position[3];rf_model_local_light lights[8]; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            rf_model_light_choice out;
            if(rf_model_choose_local_light(data.position,data.lights,8,&out)!=RF_OK)return 2;
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--vertex-lighting")) {
        struct { float vector[3],lights[3][6],ambient[3]; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            uint8_t rgb[3];int32_t status=rf_model_vertex_lighting(data.vector,data.lights,data.ambient,rgb);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(rgb,3,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--render-vertex-pair")) {
        struct { float position[3],second[3];uint8_t weights[4],bones[4];float matrices[4][12]; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            struct { int32_t status;float value[6]; } out={0,{99,99,99,99,99,99}};
            out.status=rf_model_render_vertex_pair(data.position,data.second,data.weights,data.bones,data.matrices,4,out.value);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--collision-vertex")) {
        struct { float position[3];uint8_t weights[4],bones[4];float matrices[4][12]; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            struct { int32_t status;float value[3]; } out={0,{99,99,99}};
            out.status=rf_model_collision_vertex(data.position,data.weights,data.bones,data.matrices,4,out.value);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if (argc==2 && !strcmp(argv[1],"--material-disk")) {
        struct { uint8_t raw[84]; int32_t primary,secondary; uint32_t transparent,budget; } data;
        while (fread(&data,sizeof(data),1,stdin)==1) {
            rf_model_material_instance instance={0}; int32_t status; uint32_t scalar=0;
            status=rf_model_material_from_disk(&instance,data.raw,84,data.primary,data.secondary,data.transparent,data.budget);
            if (instance.counts[1]) scalar=instance.arrays[1][0];
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&instance.record,200,1,stdout)!=1 || fwrite(&scalar,4,1,stdout)!=1) return 1;
            rf_model_material_instance_close(&instance);
        }
        return ferror(stdin) ? 1 : 0;
    }
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
