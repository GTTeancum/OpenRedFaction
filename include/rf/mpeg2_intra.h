#ifndef RF_MPEG2_INTRA_H
#define RF_MPEG2_INTRA_H
#include <stdint.h>

/* Intra-only MPEG-2 video decoder (port-owned, written from ISO/IEC 13818-2;
 * docs/FMV-PLAN.md). It covers what the PS2's movies use: I pictures only
 * (the PS2 IPU decodes nothing else), frame pictures, 4:2:0, frame or field
 * DCT, any intra DC precision, linear or non-linear quantiser scale, intra
 * VLC tables B.14 and B.15 and the zig-zag scan. P/B pictures, field
 * pictures, the alternate scan and MPEG-1 streams are refused (RF_FORMAT)
 * rather than guessed at.
 *
 * Input is one access unit at a time: the start codes from any sequence/GOP
 * headers before a picture through that picture's last slice (rf_pss splits
 * a PSS file this way). Output is the decoded picture as three 8-bit planes
 * (Y full size, U and V half size each way). */
typedef struct rf_mpeg2_intra {
    uint32_t width,height,mb_width,mb_height;
    uint32_t sequence,extension,intra_vlc_format,alternate_scan,q_scale_type,dc_precision;
    uint32_t frame_pred_frame_dct,picture_structure;
    uint8_t intra_matrix[64]; /* raster order */
    uint8_t *planes[3];uint32_t stride[3];
    uint32_t capacity_width,capacity_height;
    uint32_t pictures,skipped,errors; /* decoded, refused (not I), bitstream errors */
} rf_mpeg2_intra;

/* Allocates planes for pictures up to max_width x max_height. */
int rf_mpeg2_intra_open(rf_mpeg2_intra *decoder,uint32_t max_width,uint32_t max_height);
void rf_mpeg2_intra_close(rf_mpeg2_intra *decoder);
/* Decodes one access unit. *decoded is 1 when a picture was decoded into the
 * planes (0 for a unit holding only headers or a refused picture type). */
int rf_mpeg2_intra_decode(rf_mpeg2_intra *decoder,const uint8_t *unit,uint32_t bytes,uint32_t *decoded);
/* Converts the decoded picture to 32-bit B,G,R,A pixels (alpha 255), ITU-R
 * BT.601 studio range, into rows of pitch bytes.
 * Writes each row once front to back, so write-combined memory is fine. */
void rf_mpeg2_intra_to_bgra(const rf_mpeg2_intra *decoder,uint8_t *pixels,uint32_t pitch);
#endif
