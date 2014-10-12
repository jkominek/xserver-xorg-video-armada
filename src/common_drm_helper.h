#ifndef COMMON_DRM_HELPER_H
#define COMMON_DRM_HELPER_H

#include "xf86.h"
#include "xf86Crtc.h"
#include <xf86drm.h>

xf86CrtcPtr common_drm_covering_crtc(ScrnInfoPtr pScrn, BoxPtr box,
	xf86CrtcPtr desired, BoxPtr box_ret);

int common_drm_vblank_get(ScrnInfoPtr pScrn, xf86CrtcPtr crtc,
	drmVBlank *vbl, const char *func);

int common_drm_vblank_queue_event(ScrnInfoPtr pScrn, xf86CrtcPtr crtc,
	drmVBlank *vbl, const char *func, Bool nextonmiss, void *signal);

#endif
