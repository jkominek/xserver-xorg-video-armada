#ifndef ETNADRM_H
#define ETNADRM_H

struct etna_ctx;

void etna_emit_reloc(struct etna_ctx *ctx, uint32_t buf_offset,
	struct etna_bo *mem, uint32_t offset, Bool write);

#endif