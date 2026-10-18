/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Mikhail Teterin
 */
/*
 * GdkPixbufAnimation for Lottie; frames are rendered on demand.
 */

#ifndef IO_LOTTIE_ANIMATION_H
#define IO_LOTTIE_ANIMATION_H

#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "lottie.h"

#define LOTTIE_TYPE_ANIMATION (lottie_animation_get_type())
#define LOTTIE_ANIMATION(o)                                     \
	(G_TYPE_CHECK_INSTANCE_CAST((o), LOTTIE_TYPE_ANIMATION, \
	    LottieAnimation))

#define LOTTIE_TYPE_ANIMATION_ITER (lottie_animation_iter_get_type())
#define LOTTIE_ANIMATION_ITER(o)                                     \
	(G_TYPE_CHECK_INSTANCE_CAST((o), LOTTIE_TYPE_ANIMATION_ITER, \
	    LottieAnimationIter))

typedef struct _LottieAnimation {
	GdkPixbufAnimation parent;
	tvgpb::Lottie *lottie;
} LottieAnimation;

typedef struct _LottieAnimationClass {
	GdkPixbufAnimationClass parent_class;
} LottieAnimationClass;

typedef struct _LottieAnimationIter {
	GdkPixbufAnimationIter parent;
	LottieAnimation *anim;
	gint64 start_us;
	gint64 pos_us;
	gint64 index;
	GdkPixbuf *pixbuf;
} LottieAnimationIter;

typedef struct _LottieAnimationIterClass {
	GdkPixbufAnimationIterClass parent_class;
} LottieAnimationIterClass;

GType lottie_animation_get_type(void);
GType lottie_animation_iter_get_type(void);

/* Takes ownership of the Lottie object. */
GdkPixbufAnimation *lottie_animation_new(tvgpb::Lottie *lottie);

#endif /* IO_LOTTIE_ANIMATION_H */
