#include "rf/model_file.h"
#include <string.h>
#include <math.h>
typedef struct reader { rf_model_file *model; uint32_t cursor; int status; } reader;
static uint32_t integer(reader *r, uint32_t bytes)
{
    unsigned char raw[4] = {0};
    uint32_t value = 0, i;
    if (r->status) return 0;
    r->status = rf_vpp_read(r->model->archive, &r->model->entry, r->cursor, raw, bytes);
    if (r->status) return 0;
    r->cursor += bytes;
    for (i = 0; i < bytes; ++i) value |= (uint32_t)raw[i] << (i * 8);
    return value;
}
static void skip(reader *r, uint64_t bytes)
{
    if (r->status) return;
    if (r->cursor > r->model->entry.size || bytes > r->model->entry.size - r->cursor) r->status = RF_FORMAT;
    else r->cursor += (uint32_t)bytes;
}
static void submesh(reader *r)
{
    uint32_t version, lods, i, count;
    skip(r, 48);
    version = integer(r, 4); lods = integer(r, 4);
    if (r->status) return;
    if (version < 7 || version > INT32_MAX || lods < 1 || lods > 3) { r->status = RF_FORMAT; return; }
    skip(r, (uint64_t)lods * 4 + 40);
    for (i = 0; i < lods && !r->status; ++i) {
        uint32_t batches, bytes, textures, t, flags, auxiliary, b;
        uint64_t relative;
        rf_model_lod *lod;
        if (r->model->lod_count == RF_MODEL_MAX_LODS) { r->status = RF_RANGE; return; }
        lod = &r->model->lods[r->model->lod_count++];
        flags = integer(r, 4); auxiliary = integer(r, 4);
        batches = integer(r, 2); bytes = integer(r, 4);
        lod->offset = r->cursor; lod->size = bytes;
        skip(r, bytes); skip(r, 4);
        lod->batch_offset=r->cursor;lod->batch_count=batches;lod->flags=flags;lod->auxiliary=auxiliary;
        relative = ((uint64_t)batches * 56 + 15) & ~(uint64_t)15;
        for (b = 0; b < batches && !r->status; ++b) {
            uint32_t info[7], j;
            for (j = 0; j < 7; ++j) info[j] = integer(r, 2);
            skip(r, 4);
            /* Original 0x569920: positions, normals, UV, indices, optional
             * planes, extra data, bone links, auxiliary bytes, all aligned. */
            relative = (relative + info[2] + 15) & ~(uint64_t)15;
            relative = (relative + info[2] + 15) & ~(uint64_t)15;
            relative = (relative + info[6] + 15) & ~(uint64_t)15;
            relative = (relative + info[3] + 15) & ~(uint64_t)15;
            if (flags & 0x20) relative = (relative + (uint64_t)info[1] * 16 + 15) & ~(uint64_t)15;
            relative = (relative + info[4] + 15) & ~(uint64_t)15;
            if (info[5]) relative = (relative + info[5] + 15) & ~(uint64_t)15;
            if (flags & 1) relative = (relative + (uint64_t)auxiliary * 2 + 15) & ~(uint64_t)15;
        }
        lod->attachment_count = integer(r, 4);
        if (!r->status && relative + (uint64_t)lod->attachment_count * 100 != bytes) r->status = RF_FORMAT;
        if (r->status) return;
        lod->attachment_offset = lod->offset + (uint32_t)relative;
        textures = integer(r, 4);
        for (t = 0; t < textures && !r->status; ++t) {
            uint32_t length = 0;
            skip(r, 1);
            while (!r->status && integer(r, 1))
                if (++length > 256) r->status = RF_FORMAT;
        }
    }
    count = integer(r, 4);
    r->model->sections[r->model->section_count-1].material_count=count;
    r->model->sections[r->model->section_count-1].material_offset=r->cursor;
    skip(r, (uint64_t)count * 84);
    count = integer(r, 4); skip(r, (uint64_t)count * 28);
}
int rf_model_file_open(rf_model_file *model, rf_vpp *archive, const char *name)
{
    reader r;
    uint32_t magic, version, declared_meshes, found_meshes = 0;
    int ended = 0;
    if (!model) return RF_RANGE;
    memset(model, 0, sizeof(*model));
    if (!archive || !name) return RF_RANGE;
    model->archive = archive;
    r.model = model; r.cursor = 0;
    r.status = rf_vpp_find(archive, name, &model->entry);
    magic = integer(&r, 4); version = integer(&r, 4); declared_meshes = integer(&r, 4);
    if (!r.status && (magic != 0x5246434d || version != 0x40000)) r.status = RF_FORMAT;
    skip(&r, 28);
    while (!r.status && r.cursor < model->entry.size) {
        rf_model_section *section;
        uint32_t kind, bytes;
        if (model->section_count == RF_MODEL_MAX_SECTIONS) { r.status = RF_RANGE; break; }
        kind = integer(&r, 4); bytes = integer(&r, 4);
        if (r.status) break;
        section = &model->sections[model->section_count++];
        section->type = kind; section->offset = r.cursor;
        if (!kind) {
            if (bytes || r.cursor != model->entry.size) r.status = RF_FORMAT;
            ended = 1; break;
        }
        if (kind == 0x5355424d) { submesh(&r); ++found_meshes; }
        else skip(&r, bytes);
        section->size = r.cursor - section->offset;
    }
    if (!r.status && (!ended || found_meshes != declared_meshes)) r.status = RF_FORMAT;
    if (r.status) { memset(model, 0, sizeof(*model)); return r.status; }
    model->submeshes = found_meshes;
    return RF_OK;
}

int rf_model_file_attachment(const rf_model_file *model, uint32_t lod_index, uint32_t index, rf_model_attachment *attachment)
{
    const rf_model_lod *lod;
    unsigned char raw[100];
    rf_model_attachment value;
    uint32_t i, j;
    uint64_t offset;
    int status;
    if (!model || !model->archive || !attachment || lod_index >= model->lod_count || lod_index >= RF_MODEL_MAX_LODS) return RF_RANGE;
    lod = &model->lods[lod_index];
    if (index >= lod->attachment_count) return RF_RANGE;
    offset = (uint64_t)lod->attachment_offset + (uint64_t)index * 100;
    if (offset > UINT32_MAX || offset + 100 > (uint64_t)lod->offset + lod->size) return RF_RANGE;
    status = rf_vpp_read(model->archive, &model->entry, (uint32_t)offset, raw, 100);
    if (status) return status;
    memset(&value, 0, sizeof(value));
    memcpy(value.name, raw, 68); value.name[68] = 0;
    for (i = 0; i < 8; ++i) {
        uint32_t bits = 0;
        for (j = 0; j < 4; ++j) bits |= (uint32_t)raw[68 + i * 4 + j] << (j * 8);
        if (i == 7) memcpy(&value.parent, &bits, 4);
        else {
            float f;
            memcpy(&f, &bits, 4);
            if (!isfinite(f)) return RF_FORMAT;
            if (i < 4) value.rotation[i] = f; else value.position[i - 4] = f;
        }
    }
    if (value.parent < -1) return RF_FORMAT;
    *attachment = value;
    return RF_OK;
}
int rf_model_file_material(const rf_model_file *model,uint32_t submesh_index,uint32_t index,uint8_t raw[84])
{
    uint32_t i,n=0; uint8_t value[84];
    if (!model || !model->archive || !raw || model->section_count>RF_MODEL_MAX_SECTIONS) return RF_RANGE;
    for (i=0;i<model->section_count;++i) {
        const rf_model_section *section=&model->sections[i]; uint64_t offset,end; int status;
        if (section->type!=0x5355424d) continue;
        if (n++!=submesh_index) continue;
        if (index>=section->material_count) return RF_RANGE;
        offset=(uint64_t)section->material_offset+(uint64_t)index*84;
        end=(uint64_t)section->offset+section->size;
        if (offset<section->offset || offset>UINT32_MAX || offset+84>end) return RF_RANGE;
        status=rf_vpp_read(model->archive,&model->entry,(uint32_t)offset,value,84);
        if (status!=RF_OK) return status;
        memcpy(raw,value,84); return RF_OK;
    }
    return RF_RANGE;
}
int rf_model_file_batch(const rf_model_file *model,uint32_t lod_index,uint32_t index,rf_model_batch *batch)
{
    const rf_model_lod *lod;rf_model_batch value={0};uint64_t relative,end,descriptor;
    uint32_t i,j,info[7];uint8_t raw[18];int status;
    if(!model || !model->archive || !batch || lod_index>=model->lod_count || lod_index>=RF_MODEL_MAX_LODS) return RF_RANGE;
    lod=model->lods+lod_index;
    if(index>=lod->batch_count || lod->batch_count>65535) return RF_RANGE;
    end=(uint64_t)lod->offset+lod->size;
    if(end>model->entry.size || lod->attachment_offset<lod->offset || lod->attachment_offset>end) return RF_RANGE;
    relative=((uint64_t)lod->batch_count*56+15)&~(uint64_t)15;
    for(i=0;i<=index;++i) {
        descriptor=(uint64_t)lod->batch_offset+(uint64_t)i*18;
        if(descriptor>UINT32_MAX || descriptor+18>model->entry.size) return RF_RANGE;
        status=rf_vpp_read(model->archive,&model->entry,(uint32_t)descriptor,raw,18);if(status)return status;
        for(j=0;j<7;++j)info[j]=(uint32_t)raw[j*2]|(uint32_t)raw[j*2+1]<<8;
        memset(&value,0,sizeof(value));value.vertices=info[0];value.triangles=info[1];
        for(j=0;j<4;++j)value.format_bits|=(uint32_t)raw[14+j]<<(j*8);
        value.sizes[0]=value.sizes[1]=info[2];value.sizes[2]=info[6];value.sizes[3]=info[3];
        value.sizes[4]=(lod->flags&32)?info[1]*16:0;value.sizes[5]=info[4];value.sizes[6]=info[5];
        if((lod->flags&1) && lod->auxiliary>UINT32_MAX/2)return RF_RANGE;
        value.sizes[7]=(lod->flags&1)?lod->auxiliary*2:0;
        for(j=0;j<8;++j) {
            uint64_t offset=(uint64_t)lod->offset+relative;
            if(offset>lod->attachment_offset || value.sizes[j]>lod->attachment_offset-offset)return RF_FORMAT;
            if(value.sizes[j])value.offsets[j]=(uint32_t)offset;
            relative=(relative+value.sizes[j]+15)&~(uint64_t)15;
        }
    }
    *batch=value;return RF_OK;
}
