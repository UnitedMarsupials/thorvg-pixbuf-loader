/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Mikhail Teterin
 */

#include "io-lottie-animation.h"

G_DEFINE_TYPE(LottieAnimation, lottie_animation, GDK_TYPE_PIXBUF_ANIMATION)
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
G_DEFINE_TYPE(LottieAnimationIter, lottie_animation_iter,
    GDK_TYPE_PIXBUF_ANIMATION_ITER)
G_GNUC_END_IGNORE_DEPRECATIONS

static void
lottie_animation_init(LottieAnimation *)
{
}

static void
lottie_animation_finalize(GObject *object)
{
	LottieAnimation *anim = LOTTIE_ANIMATION(object);

	delete anim->lottie;
	anim->lottie = NULL;
	G_OBJECT_CLASS(lottie_animation_parent_class)->finalize(object);
}

static gboolean
lottie_animation_is_static_image(GdkPixbufAnimation *animation)
{
	return (LOTTIE_ANIMATION(animation)->lottie->n_frames() == 1);
}

static GdkPixbuf *
lottie_animation_get_static_image(GdkPixbufAnimation *animation)
{
	return (LOTTIE_ANIMATION(animation)->lottie->first());
}

static void
lottie_animation_get_size(GdkPixbufAnimation *animation, int *width,
    int *height)
{
	tvgpb::Lottie *lottie = LOTTIE_ANIMATION(animation)->lottie;

	if (width != NULL)
		*width = lottie->width();
	if (height != NULL)
		*height = lottie->height();
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static GdkPixbufAnimationIter *
lottie_animation_get_iter(GdkPixbufAnimation *animation,
    const GTimeVal *start_time)
{
	LottieAnimation *anim = LOTTIE_ANIMATION(animation);
	LottieAnimationIter *iter;

	iter = (LottieAnimationIter *)g_object_new(LOTTIE_TYPE_ANIMATION_ITER,
	    NULL);
	iter->anim = (LottieAnimation *)g_object_ref(anim);
	iter->start_us = (gint64)start_time->tv_sec * G_USEC_PER_SEC +
	    start_time->tv_usec;
	iter->index = 0;
	iter->pos_us = 0;
	iter->pixbuf = anim->lottie->frame(0);
	return (GDK_PIXBUF_ANIMATION_ITER(iter));
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
lottie_animation_class_init(LottieAnimationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GdkPixbufAnimationClass *anim_class = GDK_PIXBUF_ANIMATION_CLASS(klass);

	object_class->finalize = lottie_animation_finalize;
	anim_class->is_static_image = lottie_animation_is_static_image;
	anim_class->get_static_image = lottie_animation_get_static_image;
	anim_class->get_size = lottie_animation_get_size;
	anim_class->get_iter = lottie_animation_get_iter;
}

static void
lottie_animation_iter_init(LottieAnimationIter *)
{
}

static void
lottie_animation_iter_finalize(GObject *object)
{
	LottieAnimationIter *iter = LOTTIE_ANIMATION_ITER(object);

	if (iter->pixbuf != NULL)
		g_object_unref(iter->pixbuf);
	if (iter->anim != NULL)
		g_object_unref(iter->anim);
	G_OBJECT_CLASS(lottie_animation_iter_parent_class)->finalize(object);
}

static int
lottie_animation_iter_get_delay_time(GdkPixbufAnimationIter *anim_iter)
{
	LottieAnimationIter *iter = LOTTIE_ANIMATION_ITER(anim_iter);
	tvgpb::Lottie *lottie = iter->anim->lottie;
	gint64 ms;

	if (lottie->n_frames() == 1)
		return (-1);
	/* Round up, so that we never wake up before the next frame is due. */
	ms = (lottie->start_us(iter->index + 1) - iter->pos_us + 999) / 1000;
	return (ms < 1 ? 1 : (int)ms);
}

static GdkPixbuf *
lottie_animation_iter_get_pixbuf(GdkPixbufAnimationIter *anim_iter)
{
	return (LOTTIE_ANIMATION_ITER(anim_iter)->pixbuf);
}

static gboolean
lottie_animation_iter_on_currently_loading_frame(GdkPixbufAnimationIter *)
{
	return (FALSE);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static gboolean
lottie_animation_iter_advance(GdkPixbufAnimationIter *anim_iter,
    const GTimeVal *current_time)
{
	LottieAnimationIter *iter = LOTTIE_ANIMATION_ITER(anim_iter);
	tvgpb::Lottie *lottie = iter->anim->lottie;
	GdkPixbuf *pixbuf;
	gint64 now_us, elapsed_us, pos, index;

	if (lottie->n_frames() == 1)
		return (FALSE);

	now_us = (gint64)current_time->tv_sec * G_USEC_PER_SEC +
	    current_time->tv_usec;
	elapsed_us = now_us - iter->start_us;
	if (elapsed_us < 0) {
		/* The system clock was probably set backwards. */
		iter->start_us = now_us;
		elapsed_us = 0;
	}

	/* The animation loops forever. */
	pos = elapsed_us % lottie->total_us();
	index = lottie->frame_at(pos);
	iter->pos_us = pos;
	if (index == iter->index)
		return (FALSE);

	pixbuf = lottie->frame(index);
	iter->index = index;
	if (pixbuf == NULL)
		return (FALSE);
	if (iter->pixbuf != NULL)
		g_object_unref(iter->pixbuf);
	iter->pixbuf = pixbuf;
	return (TRUE);
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
lottie_animation_iter_class_init(LottieAnimationIterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	GdkPixbufAnimationIterClass *iter_class =
	    GDK_PIXBUF_ANIMATION_ITER_CLASS(klass);
	G_GNUC_END_IGNORE_DEPRECATIONS

	object_class->finalize = lottie_animation_iter_finalize;
	iter_class->get_delay_time = lottie_animation_iter_get_delay_time;
	iter_class->get_pixbuf = lottie_animation_iter_get_pixbuf;
	iter_class->on_currently_loading_frame =
	    lottie_animation_iter_on_currently_loading_frame;
	iter_class->advance = lottie_animation_iter_advance;
}

GdkPixbufAnimation *
lottie_animation_new(tvgpb::Lottie *lottie)
{
	LottieAnimation *anim;

	anim = (LottieAnimation *)g_object_new(LOTTIE_TYPE_ANIMATION, NULL);
	anim->lottie = lottie;
	return (GDK_PIXBUF_ANIMATION(anim));
}
