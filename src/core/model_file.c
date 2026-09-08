#include "rf/model_file.h"
#include <string.h>
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
        uint32_t batches, bytes, textures, t;
        skip(r, 8); batches = integer(r, 2); bytes = integer(r, 4);
        skip(r, bytes); skip(r, 4 + (uint64_t)batches * 18);
        skip(r, 4); textures = integer(r, 4);
        for (t = 0; t < textures && !r->status; ++t) {
            uint32_t length = 0;
            skip(r, 1);
            while (!r->status && integer(r, 1))
                if (++length > 256) r->status = RF_FORMAT;
        }
    }
    count = integer(r, 4); skip(r, (uint64_t)count * 84);
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
