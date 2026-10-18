/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Mikhail Teterin
 */
/*
 * gdk-pixbuf loader for Lottie animations, rendered by ThorVG.
 *
 * A file is shown as an image: a single frame is a still picture, anything
 * longer is an animation that loops forever.  The first frame is what
 * gdk_pixbuf_new_from_file() and friends return.  Being vector graphics,
 * it is rendered at whatever size the application asks for.
 */

#include <stdio.h>
#include <string.h>

#include "io-lottie-animation.h"

/* Largest file we are willing to buffer. */
#define LOTTIE_MAX_FILE (64UL * 1024 * 1024)

typedef struct {
	GdkPixbufModuleSizeFunc size_func;
	GdkPixbufModulePreparedFunc prepared_func;
	GdkPixbufModuleUpdatedFunc updated_func;
	gpointer user_data;
	GByteArray *data;
} LottieContext;

/* Whether tvg::Initializer::init() succeeded, once per process. */
static gboolean thorvg_ok;

static gboolean
check_thorvg(GError **error)
{
	if (thorvg_ok)
		return (TRUE);
	g_set_error_literal(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
	    "Cannot initialize ThorVG");
	return (FALSE);
}

static void
set_file_too_large(GError **error)
{
	g_set_error_literal(error, GDK_PIXBUF_ERROR,
	    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY, "Lottie file is too large");
}

static gpointer
lottie_begin_load(GdkPixbufModuleSizeFunc size_func,
    GdkPixbufModulePreparedFunc prepared_func,
    GdkPixbufModuleUpdatedFunc updated_func, gpointer user_data, GError **error)
{
	LottieContext *ctx;

	if (!check_thorvg(error))
		return (NULL);
	ctx = g_new0(LottieContext, 1);
	ctx->size_func = size_func;
	ctx->prepared_func = prepared_func;
	ctx->updated_func = updated_func;
	ctx->user_data = user_data;
	ctx->data = g_byte_array_new();
	return (ctx);
}

/*
 * The size is only known once the whole file is in: JSON does not promise
 * to put "w" and "h" first, and compressed files have to be inflated.
 */
static gboolean
lottie_load_increment(gpointer data, const guchar *buf, guint size,
    GError **error)
{
	LottieContext *ctx = (LottieContext *)data;

	if ((gsize)ctx->data->len + size > LOTTIE_MAX_FILE) {
		set_file_too_large(error);
		return (FALSE);
	}
	g_byte_array_append(ctx->data, buf, size);
	return (TRUE);
}

static gboolean
lottie_stop_load(gpointer data, GError **error)
{
	LottieContext *ctx = (LottieContext *)data;
	tvgpb::Lottie *lottie;
	GdkPixbufAnimation *anim;
	GdkPixbuf *pixbuf;
	gboolean ok = FALSE;
	int w, h;

	lottie = tvgpb::Lottie::open(ctx->data->data, ctx->data->len, error);
	if (lottie == NULL)
		goto out;

	/* Let the application ask for a different size, or for none. */
	w = lottie->width();
	h = lottie->height();
	if (ctx->size_func != NULL) {
		ctx->size_func(&w, &h, ctx->user_data);
		if (w == 0 || h == 0) {
			delete lottie;
			ok = TRUE;
			goto out;
		}
		if (w > 0 && h > 0)
			lottie->set_size(w, h);
	}

	anim = lottie_animation_new(lottie);
	pixbuf = lottie->first();
	if (pixbuf == NULL) {
		g_object_unref(anim);
		g_set_error_literal(error, GDK_PIXBUF_ERROR,
		    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
		    "Cannot render the first Lottie frame");
		goto out;
	}
	if (ctx->prepared_func != NULL)
		ctx->prepared_func(pixbuf, anim, ctx->user_data);
	if (ctx->updated_func != NULL)
		ctx->updated_func(pixbuf, 0, 0, gdk_pixbuf_get_width(pixbuf),
		    gdk_pixbuf_get_height(pixbuf), ctx->user_data);
	g_object_unref(anim);
	ok = TRUE;
out:
	g_byte_array_free(ctx->data, TRUE);
	g_free(ctx);
	return (ok);
}

static GdkPixbufAnimation *
lottie_load_animation(FILE *f, GError **error)
{
	tvgpb::Lottie *lottie;
	GByteArray *bytes;
	guint8 buf[65536];
	size_t n;

	if (!check_thorvg(error))
		return (NULL);
	bytes = g_byte_array_new();
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		if (bytes->len + n > LOTTIE_MAX_FILE) {
			set_file_too_large(error);
			g_byte_array_free(bytes, TRUE);
			return (NULL);
		}
		g_byte_array_append(bytes, buf, n);
	}
	if (ferror(f)) {
		g_set_error_literal(error, GDK_PIXBUF_ERROR,
		    GDK_PIXBUF_ERROR_FAILED, "Error reading Lottie file");
		g_byte_array_free(bytes, TRUE);
		return (NULL);
	}
	lottie = tvgpb::Lottie::open(bytes->data, bytes->len, error);
	g_byte_array_free(bytes, TRUE);
	if (lottie == NULL)
		return (NULL);
	return (lottie_animation_new(lottie));
}

extern "C" G_MODULE_EXPORT void
fill_vtable(GdkPixbufModule *module)
{
	/* Our GObject types live in this module and must outlive it. */
	if (module->module != NULL)
		g_module_make_resident(module->module);
	/*
	 * Without worker threads: we render on the caller's thread anyway,
	 * and a loader should not start threads in somebody else's process.
	 * The module stays loaded, so ThorVG is never shut down again.
	 */
	thorvg_ok = tvg::Initializer::init(0) == tvg::Result::Success;
	module->begin_load = lottie_begin_load;
	module->load_increment = lottie_load_increment;
	module->stop_load = lottie_stop_load;
	module->load_animation = lottie_load_animation;
}

extern "C" G_MODULE_EXPORT void
fill_info(GdkPixbufFormat *info)
{
	/*
	 * Only used where GIO cannot sniff content types.  Every Lottie file
	 * has a "layers" array, usually near the top; gzip may be Telegram's
	 * TGS, but may be anything else, too.
	 */
	static GdkPixbufModulePattern signature[] = {
		{ (char *)"*\"layers\"", (char *)"*        ", 50 },
		{ (char *)"\x1f\x8b", NULL, 10 }, { NULL, NULL, 0 }
	};
	/*
	 * gdk-pixbuf hands a file to the first loader that registers the
	 * content type GIO guesses for it, and to no other.  shared-mime-info
	 * knows no Lottie type, so GIO calls a Lottie file application/json
	 * if its name ends in .json, and text/plain without a name; TGS is
	 * application/gzip either way.  Claim those, and refuse what is not
	 * Lottie: no other loader would have taken it.
	 */
	static gchar *mime_types[] = { (gchar *)"video/lottie+json",
		(gchar *)"application/x-tgsticker", (gchar *)"application/json",
		(gchar *)"text/plain", (gchar *)"application/gzip", NULL };
	static gchar *extensions[] = { (gchar *)"json", (gchar *)"tgs", NULL };

	info->name = (gchar *)"lottie";
	info->signature = signature;
	info->description = (gchar *)"Lottie animation";
	info->mime_types = mime_types;
	info->extensions = extensions;
	info->flags = GDK_PIXBUF_FORMAT_THREADSAFE;
	info->license = (gchar *)"BSD";
}
