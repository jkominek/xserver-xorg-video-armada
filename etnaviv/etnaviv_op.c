#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "fb.h"

#include "etnaviv_accel.h"
#include "etnaviv_op.h"

#include <etnaviv/etna.h>
#include <etnaviv/etna_bo.h>
#include <etnaviv/state.xml.h>
#include <etnaviv/state_2d.xml.h>

static inline uint32_t etnaviv_src_config(struct etnaviv_format fmt,
	Bool relative)
{
	uint32_t src_cfg;

	src_cfg = VIVS_DE_SRC_CONFIG_PE10_SOURCE_FORMAT(fmt.format) |
		  VIVS_DE_SRC_CONFIG_TRANSPARENCY(0) |
		  VIVS_DE_SRC_CONFIG_LOCATION_MEMORY |
		  VIVS_DE_SRC_CONFIG_PACK_PACKED8 |
		  VIVS_DE_SRC_CONFIG_SWIZZLE(fmt.swizzle) |
		  VIVS_DE_SRC_CONFIG_SOURCE_FORMAT(fmt.format);

	if (relative)
		src_cfg |= VIVS_DE_SRC_CONFIG_SRC_RELATIVE_RELATIVE;

	if (fmt.tile)
		src_cfg |= VIVS_DE_SRC_CONFIG_TILED_ENABLE;

	return src_cfg;
}

static void etnaviv_set_source_bo(struct etna_ctx *ctx, struct etna_bo *bo,
	uint32_t pitch, struct etnaviv_format format, const xPoint *offset)
{
	size_t n_src = offset ? 5 : 4;
	uint32_t src_cfg = etnaviv_src_config(format, offset != NULL);

	etna_reserve(ctx, 1 + n_src + 1 + 2);

	ETNA_EMIT_LOAD_STATE(ctx, VIVS_DE_SRC_ADDRESS >> 2, n_src, 0);
	ETNA_EMIT(ctx, etna_bo_gpu_address(bo));
	ETNA_EMIT(ctx, VIVS_DE_SRC_STRIDE_STRIDE(pitch));
	ETNA_EMIT(ctx, VIVS_DE_SRC_ROTATION_CONFIG_ROTATION_DISABLE);
	ETNA_EMIT(ctx, src_cfg);
	if (offset)
		ETNA_EMIT(ctx,
			  VIVS_DE_SRC_ORIGIN_X(offset->x) |
			  VIVS_DE_SRC_ORIGIN_Y(offset->y));
	ETNA_ALIGN(ctx);
}

static void etnaviv_set_dest_bo(struct etna_ctx *ctx, struct etna_bo *bo,
	uint32_t pitch, struct etnaviv_format fmt, uint32_t cmd)
{
	uint32_t dst_cfg;

	dst_cfg = VIVS_DE_DEST_CONFIG_FORMAT(fmt.format) | cmd |
		  VIVS_DE_DEST_CONFIG_SWIZZLE(fmt.swizzle);

	if (fmt.tile)
		dst_cfg |= VIVS_DE_DEST_CONFIG_TILED_ENABLE;

	etna_reserve(ctx, 1 + 5);
	ETNA_EMIT_LOAD_STATE(ctx, VIVS_DE_DEST_ADDRESS >> 2, 4, 0);
	ETNA_EMIT(ctx, etna_bo_gpu_address(bo));
	ETNA_EMIT(ctx, VIVS_DE_DEST_STRIDE_STRIDE(pitch));
	ETNA_EMIT(ctx, VIVS_DE_DEST_ROTATION_CONFIG_ROTATION_DISABLE);
	ETNA_EMIT(ctx, dst_cfg);
	ETNA_ALIGN(ctx);
}

static void etnaviv_emit_rop_clip(struct etna_ctx *ctx, unsigned fg_rop,
	unsigned bg_rop, const BoxRec *clip, xPoint offset)
{
	ETNA_EMIT_LOAD_STATE(ctx, VIVS_DE_ROP >> 2, clip ? 3 : 1, 0);
	ETNA_EMIT(ctx,
		  VIVS_DE_ROP_ROP_FG(fg_rop) |
		  VIVS_DE_ROP_ROP_BG(bg_rop) |
		  VIVS_DE_ROP_TYPE_ROP4);
	if (clip) {
		ETNA_EMIT(ctx,
			  VIVS_DE_CLIP_TOP_LEFT_X(clip->x1 + offset.x) |
			  VIVS_DE_CLIP_TOP_LEFT_Y(clip->y1 + offset.y));
		ETNA_EMIT(ctx,
			  VIVS_DE_CLIP_BOTTOM_RIGHT_X(clip->x2 + offset.x) |
			  VIVS_DE_CLIP_BOTTOM_RIGHT_Y(clip->y2 + offset.y));
	}
}

static void entaviv_emit_brush(struct etna_ctx *ctx, uint32_t fg)
{
	ETNA_EMIT_LOAD_STATE(ctx, VIVS_DE_PATTERN_MASK_LOW >> 2, 4, 0);
	ETNA_EMIT(ctx, ~0);
	ETNA_EMIT(ctx, ~0);
	ETNA_EMIT(ctx, 0);
	ETNA_EMIT(ctx, fg);
	ETNA_ALIGN(ctx);
	ETNA_EMIT_LOAD_STATE(ctx, VIVS_DE_PATTERN_CONFIG >> 2, 1, 0);
	ETNA_EMIT(ctx, VIVS_DE_PATTERN_CONFIG_INIT_TRIGGER(3));
}

static void etnaviv_set_blend(struct etna_ctx *ctx,
	const struct etnaviv_blend_op *op)
{
	if (!op) {
		etna_set_state(ctx, VIVS_DE_ALPHA_CONTROL,
			       VIVS_DE_ALPHA_CONTROL_ENABLE_OFF);
	} else {
		unsigned int nr = 3 + 1;
		Bool pe20 = VIV_FEATURE(ctx->conn, chipMinorFeatures0, 2DPE20);

		if (pe20)
			nr += 4;

		etna_reserve(ctx, nr);

		ETNA_EMIT_LOAD_STATE(ctx, VIVS_DE_ALPHA_CONTROL >> 2, 2, 0);
		ETNA_EMIT(ctx,
			  VIVS_DE_ALPHA_CONTROL_ENABLE_ON |
			  VIVS_DE_ALPHA_CONTROL_PE10_GLOBAL_SRC_ALPHA(op->src_alpha) |
			  VIVS_DE_ALPHA_CONTROL_PE10_GLOBAL_DST_ALPHA(op->dst_alpha));
		ETNA_EMIT(ctx, op->alpha_mode);
		ETNA_ALIGN(ctx);

		if (pe20) {
			ETNA_EMIT_LOAD_STATE(ctx, VIVS_DE_GLOBAL_SRC_COLOR >> 2, 3, 0);
			ETNA_EMIT(ctx, op->src_alpha << 24);
			ETNA_EMIT(ctx, op->dst_alpha << 24);
			ETNA_EMIT(ctx,
				  VIVS_DE_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE |
				  VIVS_DE_COLOR_MULTIPLY_MODES_DST_PREMULTIPLY_DISABLE |
				  VIVS_DE_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_DISABLE |
				  VIVS_DE_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_DISABLE);
		}
	}
}

static void etnaviv_emit_2d_draw(struct etna_ctx *ctx, const BoxRec *pbox,
	size_t n, xPoint offset)
{
	size_t i;

	assert(n);

	ETNA_EMIT(ctx,
		  VIV_FE_DRAW_2D_HEADER_OP_DRAW_2D |
		  VIV_FE_DRAW_2D_HEADER_DATA_COUNT(0) |
		  VIV_FE_DRAW_2D_HEADER_COUNT(n));
	ETNA_EMIT(ctx, 0);

	for (i = 0; i < n; i++, pbox++) {
		ETNA_EMIT(ctx,
			  VIV_FE_DRAW_2D_TOP_LEFT_X(offset.x + pbox->x1) |
			  VIV_FE_DRAW_2D_TOP_LEFT_Y(offset.y + pbox->y1));
		ETNA_EMIT(ctx,
			  VIV_FE_DRAW_2D_BOTTOM_RIGHT_X(offset.x + pbox->x2) |
			  VIV_FE_DRAW_2D_BOTTOM_RIGHT_Y(offset.y + pbox->y2));
	}
}

void etnaviv_de_op(struct etnaviv *etnaviv, const struct etnaviv_de_op *op,
	const BoxRec *pBox, size_t nBox)
{
	struct etna_ctx *ctx = etnaviv->ctx;
	size_t n;

	assert(nBox <= VIVANTE_MAX_2D_RECTS);

	n = 6 + 2 + 2 + 2 + nBox * 2;
	if (op->src.bo)
		n += 8;
	if (op->blend_op)
		n += 8 - 2;
	if (op->brush)
		n += 8;
	if (op->clip)
		n += 2;

	/* Reserve for the whole operation in one go */
	etna_reserve(ctx, n);

	if (op->src.bo)
		etnaviv_set_source_bo(ctx, op->src.bo, op->src.pitch,
				      op->src.format, &op->src.offset);
	etnaviv_set_dest_bo(ctx, op->dst.bo, op->dst.pitch, op->dst.format,
			    op->cmd);
	if (op->brush)
		entaviv_emit_brush(ctx, op->fg_colour);
	etnaviv_set_blend(ctx, op->blend_op);
	etnaviv_emit_rop_clip(ctx, op->rop, op->rop, op->clip, op->dst.offset);
	etnaviv_emit_2d_draw(ctx, pBox, nBox, op->dst.offset);
}

void etnaviv_flush(struct etnaviv *etnaviv)
{
	struct etna_ctx *ctx = etnaviv->ctx;
	etna_set_state(ctx, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_PE2D);
}
