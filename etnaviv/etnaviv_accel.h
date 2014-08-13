/*
 * Vivante GPU Acceleration Xorg driver
 *
 * Written by Russell King, 2012, derived in part from the
 * Intel xorg X server driver.
 */
#ifndef VIVANTE_ACCEL_H
#define VIVANTE_ACCEL_H

#include <etnaviv/viv.h>
#include <etnaviv/etna.h>
#include <etnaviv/etna_bo.h>
#include "compat-list.h"
#include "pixmaputil.h"
#include "etnaviv_compat.h"
#include "etnaviv_op.h"

struct drm_armada_bo;
struct drm_armada_bufmgr;
struct etnaviv_dri2_info;

#undef DEBUG

/* Debugging options */
#define DEBUG_CHECK_DRAWABLE_USE
#undef DEBUG_MAP
#undef DEBUG_PIXMAP

/* Accelerated operations debugging */
#undef DEBUG_COPYNTON
#undef DEBUG_FILLSPANS
#undef DEBUG_POLYFILLRECT
#undef DEBUG_PUTIMAGE


/* Debugging */
#define OP_NOP 0
#define OP_USER_INV 1
#define OP_USER_CLN 2
#define OP_USER_FLS 3
#define OP_KERN_INV 5
#define OP_KERN_CLN 6
#define OP_KERN_FLS 7

#define dbg(fmt...) fprintf(stderr, fmt)

/* Private CreatePixmap usage hints. */
enum {
	CREATE_PIXMAP_USAGE_TILE = 0x80000000,
};

struct etnaviv {
	struct viv_conn *conn;
	struct etna_ctx *ctx;
	struct xorg_list pending_list;
	Bool need_stall;
	Bool need_commit;
	Bool force_fallback;
	struct drm_armada_bufmgr *bufmgr;
	int scrnIndex;
#ifdef HAVE_DRI2
	Bool dri2_enabled;
	Bool dri2_armada;
	struct etnaviv_dri2_info *dri2;
#endif

	CloseScreenProcPtr CloseScreen;
	GetImageProcPtr GetImage;
	GetSpansProcPtr GetSpans;
	ChangeWindowAttributesProcPtr ChangeWindowAttributes;
	CopyWindowProcPtr CopyWindow;
	CreatePixmapProcPtr CreatePixmap;
	DestroyPixmapProcPtr DestroyPixmap;
	CreateGCProcPtr CreateGC;
	BitmapToRegionProcPtr BitmapToRegion;
	ScreenBlockHandlerProcPtr BlockHandler;

	CompositeProcPtr Composite;
	GlyphsProcPtr Glyphs;
	TrapezoidsProcPtr Trapezoids;
	TrianglesProcPtr Triangles;
	AddTrianglesProcPtr AddTriangles;
	AddTrapsProcPtr AddTraps;
	UnrealizeGlyphProcPtr UnrealizeGlyph;
};

struct etnaviv_pixmap {
	uint16_t width;
	uint16_t height;
	unsigned pitch;
	struct etnaviv_format format;
	struct etnaviv_format pict_format;
	viv_usermem_t info;
	struct xorg_list pending_node;
	Bool need_stall;

	uint8_t state;
#define ST_DMABUF	(1 << 4)

	enum {
		NONE,
		CPU,
		GPU,
	} owner;
#ifdef DEBUG_CHECK_DRAWABLE_USE
	int in_use;
#endif
	struct drm_armada_bo *bo;
	struct etna_bo *etna_bo;
	uint32_t name;
};

/* Addresses must be aligned */
#define VIVANTE_ALIGN_MASK	63

/* 2D acceleration */
Bool etnaviv_accel_FillSpans(DrawablePtr pDrawable, GCPtr pGC, int n,
	DDXPointPtr ppt, int *pwidth, int fSorted);
Bool etnaviv_accel_PutImage(DrawablePtr pDrawable, GCPtr pGC, int depth,
	int x, int y, int w, int h, int leftPad, int format, char *bits);
void etnaviv_accel_CopyNtoN(DrawablePtr pSrc, DrawablePtr pDst,
	GCPtr pGC, BoxPtr pBox, int nBox, int dx, int dy, Bool reverse,
	Bool upsidedown, Pixel bitPlane, void *closure);
Bool etnaviv_accel_PolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode,
	int npt, DDXPointPtr ppt);
Bool etnaviv_accel_PolyFillRectSolid(DrawablePtr pDrawable, GCPtr pGC, int n,
	xRectangle * prect);
Bool etnaviv_accel_PolyFillRectTiled(DrawablePtr pDrawable, GCPtr pGC, int n,
	xRectangle * prect);

/* 3D acceleration */
int etnaviv_accel_Composite(CARD8 op, PicturePtr pSrc, PicturePtr pMask,
	PicturePtr pDst, INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask,
	INT16 xDst, INT16 yDst, CARD16 width, CARD16 height);

void etnaviv_commit(struct etnaviv *etnaviv, Bool stall);

void etnaviv_batch_wait_commit(struct etnaviv *etnaviv, struct etnaviv_pixmap *vPix);

void etnaviv_accel_shutdown(struct etnaviv *);
Bool etnaviv_accel_init(struct etnaviv *);

static inline struct etnaviv_pixmap *etnaviv_get_pixmap_priv(PixmapPtr pixmap)
{
	extern etnaviv_Key etnaviv_pixmap_index;
	return etnaviv_GetKeyPriv(&pixmap->devPrivates, &etnaviv_pixmap_index);
}

static inline struct etnaviv_pixmap *etnaviv_drawable_offset(
	DrawablePtr pDrawable, xPoint *offset)
{
	PixmapPtr pix = drawable_pixmap_offset(pDrawable, offset);
	return etnaviv_get_pixmap_priv(pix);
}

static inline struct etnaviv *etnaviv_get_screen_priv(ScreenPtr pScreen)
{
	extern etnaviv_Key etnaviv_screen_index;
	return etnaviv_GetKeyPriv(&pScreen->devPrivates, &etnaviv_screen_index);
}

static inline void etnaviv_set_pixmap_priv(PixmapPtr pixmap, struct etnaviv_pixmap *g)
{
	extern etnaviv_Key etnaviv_pixmap_index;
	dixSetPrivate(&pixmap->devPrivates, &etnaviv_pixmap_index, g);
}

static inline void etnaviv_set_screen_priv(ScreenPtr pScreen, struct etnaviv *g)
{
	extern etnaviv_Key etnaviv_screen_index;
	dixSetPrivate(&pScreen->devPrivates, &etnaviv_screen_index, g);
}

Bool etnaviv_pixmap_flink(PixmapPtr pixmap, uint32_t *name);

#endif
