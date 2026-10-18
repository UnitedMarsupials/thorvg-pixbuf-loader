/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Mikhail Teterin
 */
/*
 * Exercise the Lottie loader through the public gdk-pixbuf API.
 *
 * usage: test-loader <data-dir>
 *
 * GDK_PIXBUF_MODULE_FILE must point to a loaders cache that lists the
 * freshly built module.
 */

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOLERANCE 10

static int failures;

#define CHECK(cond, ...)                                                     \
	do {                                                                 \
		if (!(cond)) {                                               \
			failures++;                                          \
			fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
			fprintf(stderr, __VA_ARGS__);                        \
			fprintf(stderr, "\n");                               \
		}                                                            \
	} while (0)

static gboolean
near(int a, int b)
{
	return (abs(a - b) <= TOLERANCE);
}

/* Is the pixel at (x, y) (r, g, b, a)? */
static gboolean
is_rgba_at(GdkPixbuf *pixbuf, int x, int y, int r, int g, int b, int a)
{
	const guchar *p;

	if (gdk_pixbuf_get_n_channels(pixbuf) != 4) {
		fprintf(stderr, "  pixbuf has no alpha channel\n");
		return (FALSE);
	}
	p = gdk_pixbuf_get_pixels(pixbuf) +
	    y * gdk_pixbuf_get_rowstride(pixbuf) + x * 4;
	if (!near(p[0], r) || !near(p[1], g) || !near(p[2], b) ||
	    !near(p[3], a)) {
		fprintf(stderr,
		    "  pixel (%d, %d) is (%d, %d, %d, %d), "
		    "want (%d, %d, %d, %d)\n",
		    x, y, p[0], p[1], p[2], p[3], r, g, b, a);
		return (FALSE);
	}
	return (TRUE);
}

/* Is the middle pixel opaque (r, g, b)? */
static gboolean
is_color(GdkPixbuf *pixbuf, int r, int g, int b)
{
	return (is_rgba_at(pixbuf, gdk_pixbuf_get_width(pixbuf) / 2,
	    gdk_pixbuf_get_height(pixbuf) / 2, r, g, b, 255));
}

static char *
data_path(const char *dir, const char *file)
{
	return (g_build_filename(dir, file, NULL));
}

static GdkPixbuf *
load(const char *dir, const char *file)
{
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	char *path;

	path = data_path(dir, file);
	pixbuf = gdk_pixbuf_new_from_file(path, &error);
	CHECK(pixbuf != NULL, "%s: %s", file,
	    error != NULL ? error->message : "?");
	g_clear_error(&error);
	g_free(path);
	return (pixbuf);
}

/* Feed a whole buffer to a loader; returns its pixbuf or NULL. */
static GdkPixbuf *
load_bytes(GdkPixbufLoader *loader, const char *data, gsize len, GError **error)
{
	GdkPixbuf *pixbuf;

	if (!gdk_pixbuf_loader_write(loader, (const guchar *)data, len,
		error)) {
		gdk_pixbuf_loader_close(loader, NULL);
		return (NULL);
	}
	if (!gdk_pixbuf_loader_close(loader, error))
		return (NULL);
	pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
	return (pixbuf != NULL ? g_object_ref(pixbuf) : NULL);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
test_still(const char *dir)
{
	GdkPixbufAnimation *anim;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	char *path;

	pixbuf = load(dir, "still.json");
	if (pixbuf != NULL) {
		CHECK(gdk_pixbuf_get_width(pixbuf) == 64 &&
			gdk_pixbuf_get_height(pixbuf) == 48,
		    "still.json: size is %dx%d", gdk_pixbuf_get_width(pixbuf),
		    gdk_pixbuf_get_height(pixbuf));
		CHECK(is_color(pixbuf, 0, 0, 255), "still.json: not blue");
		g_object_unref(pixbuf);
	}

	path = data_path(dir, "still.json");
	anim = gdk_pixbuf_animation_new_from_file(path, &error);
	CHECK(anim != NULL, "still.json: animation: %s",
	    error != NULL ? error->message : "?");
	g_clear_error(&error);
	if (anim != NULL) {
		CHECK(gdk_pixbuf_animation_is_static_image(anim),
		    "still.json: not a static image");
		g_object_unref(anim);
	}
	g_free(path);
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
test_alpha(const char *dir)
{
	GdkPixbuf *pixbuf;

	pixbuf = load(dir, "alpha.json");
	if (pixbuf == NULL)
		return;
	/* Not premultiplied: the colour stays red at half opacity. */
	CHECK(is_rgba_at(pixbuf, 32, 24, 255, 0, 0, 128),
	    "alpha.json: not half-transparent red");
	g_object_unref(pixbuf);
}

/* A vector picture is rendered at the size asked for, stretched if need be. */
static void
test_scale(const char *dir)
{
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	char *path;

	path = data_path(dir, "still.json");
	pixbuf = gdk_pixbuf_new_from_file_at_scale(path, 100, 30, FALSE,
	    &error);
	CHECK(pixbuf != NULL, "still.json at 100x30: %s",
	    error != NULL ? error->message : "?");
	g_clear_error(&error);
	if (pixbuf != NULL) {
		CHECK(gdk_pixbuf_get_width(pixbuf) == 100 &&
			gdk_pixbuf_get_height(pixbuf) == 30,
		    "still.json: scaled size is %dx%d",
		    gdk_pixbuf_get_width(pixbuf),
		    gdk_pixbuf_get_height(pixbuf));
		CHECK(is_rgba_at(pixbuf, 1, 1, 0, 0, 255, 255) &&
			is_rgba_at(pixbuf, 98, 28, 0, 0, 255, 255),
		    "still.json: scaled picture does not fill the pixbuf");
		g_object_unref(pixbuf);
	}
	g_free(path);
}

static void
test_file_info(const char *dir)
{
	GdkPixbufFormat *format;
	char *path, *name;
	int w = 0, h = 0;

	path = data_path(dir, "rgb.json");
	format = gdk_pixbuf_get_file_info(path, &w, &h);
	CHECK(format != NULL, "rgb.json: no file info");
	if (format != NULL) {
		name = gdk_pixbuf_format_get_name(format);
		CHECK(strcmp(name, "lottie") == 0, "rgb.json: format is %s",
		    name);
		g_free(name);
		CHECK(w == 64 && h == 48, "rgb.json: info size is %dx%d", w, h);
	}
	g_free(path);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
/* The picture an iterator shows ms milliseconds after it was created. */
static GdkPixbuf *
frame_at(GdkPixbufAnimationIter *iter, const GTimeVal *start, int ms)
{
	GTimeVal now = *start;

	g_time_val_add(&now, (glong)ms * 1000);
	gdk_pixbuf_animation_iter_advance(iter, &now);
	return (gdk_pixbuf_animation_iter_get_pixbuf(iter));
}

static void
test_animation(const char *dir, const char *file)
{
	GdkPixbufAnimation *anim;
	GdkPixbufAnimationIter *iter;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	GTimeVal start;
	char *path;

	path = data_path(dir, file);
	anim = gdk_pixbuf_animation_new_from_file(path, &error);
	g_free(path);
	CHECK(anim != NULL, "%s: animation: %s", file,
	    error != NULL ? error->message : "?");
	g_clear_error(&error);
	if (anim == NULL)
		return;
	CHECK(!gdk_pixbuf_animation_is_static_image(anim),
	    "%s: is a static image", file);
	CHECK(gdk_pixbuf_animation_get_width(anim) == 64 &&
		gdk_pixbuf_animation_get_height(anim) == 48,
	    "%s: animation size", file);
	CHECK(is_color(gdk_pixbuf_animation_get_static_image(anim), 255, 0, 0),
	    "%s: static image is not red", file);

	g_get_current_time(&start);
	iter = gdk_pixbuf_animation_get_iter(anim, &start);
	pixbuf = gdk_pixbuf_animation_iter_get_pixbuf(iter);
	CHECK(is_color(pixbuf, 255, 0, 0), "%s: 0 ms is not red", file);
	/* Three frames of 100 ms each are red. */
	CHECK(gdk_pixbuf_animation_iter_get_delay_time(iter) == 100,
	    "%s: delay is %d ms", file,
	    gdk_pixbuf_animation_iter_get_delay_time(iter));
	CHECK(is_color(frame_at(iter, &start, 350), 0, 255, 0),
	    "%s: 350 ms is not green", file);
	CHECK(gdk_pixbuf_animation_iter_get_delay_time(iter) == 50,
	    "%s: delay at 350 ms is %d ms", file,
	    gdk_pixbuf_animation_iter_get_delay_time(iter));
	CHECK(is_color(frame_at(iter, &start, 550), 0, 0, 255),
	    "%s: 550 ms is not blue", file);
	/* Total length is 600 ms; it loops. */
	CHECK(is_color(frame_at(iter, &start, 650), 255, 0, 0),
	    "%s: 650 ms is not red", file);
	CHECK(is_color(frame_at(iter, &start, 150), 255, 0, 0),
	    "%s: back to 150 ms is not red", file);
	CHECK(is_color(frame_at(iter, &start, 1150), 0, 0, 255),
	    "%s: 1150 ms is not blue", file);
	g_object_unref(iter);
	g_object_unref(anim);
}
G_GNUC_END_IGNORE_DEPRECATIONS

/*
 * Data without a file name, as an application like Pidgin hands it over:
 * GIO calls gzip application/gzip, and JSON text/plain.
 */
static void
test_nameless(const char *dir)
{
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	char *path, *data;
	gsize len;

	path = data_path(dir, "rgb.tgs");
	if (g_file_get_contents(path, &data, &len, &error)) {
		loader = gdk_pixbuf_loader_new();
		pixbuf = load_bytes(loader, data, len, &error);
		CHECK(pixbuf != NULL, "rgb.tgs without a name: %s",
		    error != NULL ? error->message : "?");
		if (pixbuf != NULL) {
			CHECK(is_color(pixbuf, 255, 0, 0),
			    "rgb.tgs without a name: not red");
			g_object_unref(pixbuf);
		}
		g_object_unref(loader);
		g_free(data);
	} else
		CHECK(FALSE, "%s: %s", path, error->message);
	g_clear_error(&error);
	g_free(path);

	path = data_path(dir, "rgb.json");
	if (g_file_get_contents(path, &data, &len, &error)) {
		loader = gdk_pixbuf_loader_new();
		pixbuf = load_bytes(loader, data, len, &error);
		CHECK(pixbuf != NULL, "rgb.json without a name: %s",
		    error != NULL ? error->message : "?");
		if (pixbuf != NULL) {
			CHECK(is_color(pixbuf, 255, 0, 0),
			    "rgb.json without a name: not red");
			g_object_unref(pixbuf);
		}
		g_object_unref(loader);
		g_clear_error(&error);

		loader = gdk_pixbuf_loader_new_with_mime_type(
		    "video/lottie+json", &error);
		CHECK(loader != NULL, "video/lottie+json: %s",
		    error != NULL ? error->message : "?");
		if (loader != NULL) {
			pixbuf = load_bytes(loader, data, len, &error);
			CHECK(pixbuf != NULL, "rgb.json by MIME type: %s",
			    error != NULL ? error->message : "?");
			if (pixbuf != NULL)
				g_object_unref(pixbuf);
			g_object_unref(loader);
		}
		g_free(data);
	} else
		CHECK(FALSE, "%s: %s", path, error->message);
	g_clear_error(&error);
	g_free(path);
}

/* Files that must be refused, without crashing. */
static void
test_bad(const char *dir)
{
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	char *path, *data;
	gsize len;

	path = data_path(dir, "not-lottie.json");
	pixbuf = gdk_pixbuf_new_from_file(path, &error);
	CHECK(pixbuf == NULL && error != NULL,
	    "not-lottie.json: loaded anyway");
	if (pixbuf != NULL)
		g_object_unref(pixbuf);
	g_clear_error(&error);
	g_free(path);

	path = data_path(dir, "rgb.json");
	if (g_file_get_contents(path, &data, &len, &error)) {
		loader = gdk_pixbuf_loader_new_with_type("lottie", NULL);
		pixbuf = load_bytes(loader, data, len / 2, &error);
		CHECK(pixbuf == NULL && error != NULL,
		    "truncated rgb.json: loaded anyway");
		if (pixbuf != NULL)
			g_object_unref(pixbuf);
		g_object_unref(loader);
		g_free(data);
	} else
		CHECK(FALSE, "%s: %s", path, error->message);
	g_clear_error(&error);
	g_free(path);
}

/*
 * A Lottie image layer showing a 64x48 red PNG, given either as a data: URI
 * or as a file name.  Untrusted files must not make us open local files, so
 * only the embedded image may show up.
 */
static GdkPixbuf *
load_image_layer(const char *u, const char *p, int embedded)
{
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	char *json;

	json =
	    g_strdup_printf("{\"v\":\"5.7.0\",\"fr\":10,\"ip\":0,\"op\":1,"
			    "\"w\":64,\"h\":48,\"ddd\":0,\"assets\":[{\"id\":"
			    "\"img\",\"w\":64,\"h\":48,\"u\":\"%s\",\"p\":"
			    "\"%s\",\"e\":%d}],\"layers\":[{\"ddd\":0,"
			    "\"ind\":1,\"ty\":2,\"refId\":\"img\",\"sr\":1,"
			    "\"ks\":{\"o\":{\"a\":0,\"k\":100},\"r\":{\"a\":"
			    "0,\"k\":0},\"p\":{\"a\":0,\"k\":[0,0,0]},\"a\":"
			    "{\"a\":0,\"k\":[0,0,0]},\"s\":{\"a\":0,\"k\":"
			    "[100,100,100]}},\"ao\":0,\"ip\":0,\"op\":1,"
			    "\"st\":0,\"bm\":0}]}",
		u, p, embedded);
	loader = gdk_pixbuf_loader_new_with_type("lottie", NULL);
	pixbuf = load_bytes(loader, json, strlen(json), &error);
	CHECK(pixbuf != NULL, "image layer: %s",
	    error != NULL ? error->message : "?");
	g_clear_error(&error);
	g_object_unref(loader);
	g_free(json);
	return (pixbuf);
}

/*
 * A 64x48 opaque red PNG.  The loaders cache used by the tests knows only
 * the Lottie loader, so gdk-pixbuf cannot make one here.
 */
/* clang-format off */
static const guchar red_png[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x30,
	0x08, 0x06, 0x00, 0x00, 0x00, 0xa1, 0x4b, 0x7c, 0x1f, 0x00, 0x00, 0x00,
	0x04, 0x73, 0x42, 0x49, 0x54, 0x08, 0x08, 0x08, 0x08, 0x7c, 0x08, 0x64,
	0x88, 0x00, 0x00, 0x00, 0x50, 0x49, 0x44, 0x41, 0x54, 0x68, 0xde, 0xed,
	0xd0, 0x01, 0x01, 0x00, 0x00, 0x04, 0x03, 0xb0, 0xd3, 0xbf, 0x33, 0x3d,
	0xd8, 0x22, 0xac, 0x26, 0x99, 0x3c, 0xd6, 0x79, 0x4e, 0x80, 0x00, 0x01,
	0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00, 0x01, 0x02, 0x04, 0x08,
	0x10, 0x20, 0x40, 0x80, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
	0x80, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00, 0x01,
	0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00, 0x01, 0x02, 0x04, 0x5c,
	0xb4, 0x39, 0xfc, 0x02, 0x5e, 0x29, 0x8a, 0x00, 0x9f, 0x00, 0x00, 0x00,
	0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
/* clang-format on */

static void
test_external_image(void)
{
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	char *tmp, *png, *b64, *uri, *u;

	tmp = g_dir_make_tmp("test-loader-XXXXXX", &error);
	if (tmp == NULL) {
		CHECK(FALSE, "temporary directory: %s", error->message);
		g_clear_error(&error);
		return;
	}
	png = g_build_filename(tmp, "red.png", NULL);
	CHECK(g_file_set_contents(png, (const char *)red_png, sizeof(red_png),
		  &error),
	    "%s: %s", png, error != NULL ? error->message : "?");
	g_clear_error(&error);

	/* Embedded, it is drawn: image layers do work. */
	b64 = g_base64_encode(red_png, sizeof(red_png));
	uri = g_strconcat("data:image/png;base64,", b64, NULL);
	pixbuf = load_image_layer("", uri, 1);
	if (pixbuf != NULL) {
		CHECK(is_color(pixbuf, 255, 0, 0),
		    "embedded image is not drawn");
		g_object_unref(pixbuf);
	}

	/* By absolute path, and by a relative one climbing up to it. */
	u = g_strconcat(tmp, "/", NULL);
	pixbuf = load_image_layer(u, "red.png", 0);
	if (pixbuf != NULL) {
		CHECK(is_rgba_at(pixbuf, 32, 24, 0, 0, 0, 0),
		    "image read from an absolute path");
		g_object_unref(pixbuf);
	}
	g_free(u);
	u = g_strconcat("../../../../../../../../../../..", tmp, "/", NULL);
	pixbuf = load_image_layer(u, "red.png", 0);
	if (pixbuf != NULL) {
		CHECK(is_rgba_at(pixbuf, 32, 24, 0, 0, 0, 0),
		    "image read from a relative path");
		g_object_unref(pixbuf);
	}
	g_free(u);

	g_unlink(png);
	g_rmdir(tmp);
	g_free(uri);
	g_free(b64);
	g_free(png);
	g_free(tmp);
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s data-dir\n", argv[0]);
		return (2);
	}
	test_still(argv[1]);
	test_alpha(argv[1]);
	test_scale(argv[1]);
	test_file_info(argv[1]);
	test_animation(argv[1], "rgb.json");
	test_animation(argv[1], "rgb.tgs");
	test_nameless(argv[1]);
	test_bad(argv[1]);
	test_external_image();
	if (failures != 0) {
		fprintf(stderr, "%d failures\n", failures);
		return (1);
	}
	printf("all tests passed\n");
	return (0);
}
