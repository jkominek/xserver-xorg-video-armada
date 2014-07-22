/*
 * Vivante GPU Acceleration Xorg driver
 *
 * Written by Russell King, 2012, derived in part from the
 * Intel xorg X server driver.
 */
#ifndef VIVANTE_UTILS_H
#define VIVANTE_UTILS_H

#include "utils.h"

#include <etnaviv/viv.h>

struct etnaviv;
struct etnaviv_pixmap;

const char *etnaviv_strerror(int err);
#define etnaviv_error(v,w,e) __etnaviv_error(v,__func__,w,e)
void __etnaviv_error(struct etnaviv *, const char *, const char *, int);

Bool etnaviv_map_gpu(struct etnaviv *etnaviv, struct etnaviv_pixmap *vPix);

struct etnaviv_format etnaviv_pict_format(PictFormatShort format, Bool force);
Bool etnaviv_src_format_valid(struct etnaviv *, struct etnaviv_format fmt);
Bool etnaviv_dst_format_valid(struct etnaviv *, struct etnaviv_format fmt);

void dump_Drawable(DrawablePtr pDraw, const char *, ...)
	__attribute__((__format__(__printf__, 2, 3)));
void dump_Picture(PicturePtr pDst, const char *, ...)
	__attribute__((__format__(__printf__, 2, 3)));
void dump_vPix(struct etnaviv *etnaviv, struct etnaviv_pixmap *vPix,
	int alpha, const char *fmt, ...)
	__attribute__((__format__(__printf__, 4, 5)));

static inline unsigned int etnaviv_pitch(unsigned width, unsigned bpp)
{
	unsigned pitch = bpp != 4 ? width * ((bpp + 7) / 8) : width / 2;

	/* GC320 and GC600 needs pitch aligned to 16 */
	return ALIGN(pitch, 16);
}

#endif
