// SPDX-License-Identifier: BSD-2-Clause
//
// Copyright (c) 2026 Mikhail Teterin

#include <cmath>
#include <new>

#include "lottie.h"

namespace tvgpb {

namespace {

// Longer animations, or more frames, mean a broken or hostile file.
constexpr gint64 kMaxTotalUs = 24LL * 3600 * G_USEC_PER_SEC;
constexpr gint64 kMaxFrames = 1000 * 1000;

// ThorVG resolves a Lottie image asset that is not embedded as
// <rpath>/<u><p> and opens it, whatever the file says <u> and <p> are.
// Nothing can be below /dev/null, not even "..", so no such path exists.
const char kNoAssets[] = "/dev/null";

void
set_error(GError **error, GdkPixbufError code, const char *msg)
{
	g_set_error_literal(error, GDK_PIXBUF_ERROR, code, msg);
}

bool
size_ok(guint64 w, guint64 h)
{
	return (w > 0 && h > 0 && w <= TVGPB_MAX_DIMENSION &&
	    h <= TVGPB_MAX_DIMENSION && w * h <= TVGPB_MAX_PIXELS);
}

} // namespace

Lottie *
Lottie::open(const guint8 *data, gsize len, GError **error)
{
	std::unique_ptr<Lottie> l(new (std::nothrow) Lottie());
	tvg::Picture *picture;
	tvg::Result r;
	float w, h, total, duration;

	if (l == nullptr) {
		set_error(error, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
		    "Cannot allocate the Lottie animation");
		return (nullptr);
	}
	if (len == 0 || len > G_MAXUINT32) {
		set_error(error, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
		    "Lottie file is empty or too large");
		return (nullptr);
	}
	l->anim_.reset(tvg::Animation::gen());
	l->canvas_.reset(tvg::SwCanvas::gen());
	if (l->anim_ == nullptr || l->canvas_ == nullptr) {
		set_error(error, GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
		    "Cannot create the ThorVG canvas");
		return (nullptr);
	}
	picture = l->anim_->picture();

	// Name the type, so that ThorVG never tries its other loaders on
	// what it is given here; copy, as the data is not NUL-terminated.
	r = picture->load((const char *)data, (uint32_t)len, "lottie+json",
	    kNoAssets, true);
	if (r != tvg::Result::Success) {
		set_error(error, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
		    "Not a valid Lottie animation");
		return (nullptr);
	}
	if (picture->size(&w, &h) != tvg::Result::Success ||
	    !std::isfinite(w) || !std::isfinite(h) || w < 1 || h < 1 ||
	    !size_ok((guint64)std::ceil(w), (guint64)std::ceil(h))) {
		set_error(error, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
		    "Lottie animation has an unsupported size");
		return (nullptr);
	}
	l->natural_w_ = w;
	l->natural_h_ = h;
	l->width_ = (int)std::ceil(w);
	l->height_ = (int)std::ceil(h);

	// Every Lottie file has an out-point beyond its in-point.  Anything
	// that does not add up to at least two frames over a positive, finite
	// time (no frame rate, for one) is shown as a still picture.
	total = l->anim_->totalFrame();
	duration = l->anim_->duration();
	if (!(total >= 1)) {
		set_error(error, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
		    "Lottie animation has no frames");
		return (nullptr);
	}
	if (std::isfinite(total) && std::isfinite(duration) && total >= 2 &&
	    duration > 0) {
		if (total > kMaxFrames ||
		    duration * G_USEC_PER_SEC > kMaxTotalUs) {
			set_error(error, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			    "Lottie animation is too long");
			return (nullptr);
		}
		l->n_frames_ = (gint64)total;
		l->total_us_ = (gint64)std::llround(
		    (double)duration * G_USEC_PER_SEC);
		if (l->total_us_ < l->n_frames_) {
			set_error(error, GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			    "Lottie frame rate is too high");
			return (nullptr);
		}
	}

	// The canvas takes a reference to the picture; the animation keeps
	// its own.
	if (l->canvas_->add(picture) != tvg::Result::Success) {
		set_error(error, GDK_PIXBUF_ERROR_FAILED,
		    "Cannot add the Lottie picture to the canvas");
		return (nullptr);
	}
	return (l.release());
}

Lottie::~Lottie()
{
	if (first_ != nullptr)
		g_object_unref(first_);
	if (last_ != nullptr)
		g_object_unref(last_);
	// The canvas goes before the animation that owns the picture.
	canvas_.reset();
	anim_.reset();
}

void
Lottie::set_size(int width, int height)
{
	std::lock_guard<std::mutex> lock(mu_);
	tvg::Matrix m = { };

	if (width == width_ && height == height_)
		return;
	if (!size_ok((guint64)width, (guint64)height))
		return;
	width_ = width;
	height_ = height;
	// Stretch to exactly the size asked for, as other loaders do;
	// Picture::size() would keep the aspect ratio.
	m.e11 = width / natural_w_;
	m.e22 = height / natural_h_;
	m.e33 = 1;
	anim_->picture()->transform(m);
	if (first_ != nullptr) {
		g_object_unref(first_);
		first_ = nullptr;
	}
	if (last_ != nullptr) {
		g_object_unref(last_);
		last_ = nullptr;
	}
	last_index_ = -1;
}

GdkPixbuf *
Lottie::first()
{
	std::lock_guard<std::mutex> lock(mu_);

	if (first_ == nullptr)
		first_ = render(0);
	return (first_);
}

GdkPixbuf *
Lottie::frame(gint64 i)
{
	std::lock_guard<std::mutex> lock(mu_);
	GdkPixbuf *pixbuf;

	if (i < 0 || i >= n_frames_)
		return (nullptr);
	if (i == 0 && first_ != nullptr)
		return ((GdkPixbuf *)g_object_ref(first_));
	if (i == last_index_ && last_ != nullptr)
		return ((GdkPixbuf *)g_object_ref(last_));
	pixbuf = render(i);
	if (pixbuf == nullptr)
		return (nullptr);
	if (last_ != nullptr)
		g_object_unref(last_);
	last_ = (GdkPixbuf *)g_object_ref(pixbuf);
	last_index_ = i;
	return (pixbuf);
}

// Render frame i into a new pixbuf.  Called with mu_ held.  Each frame gets
// a pixbuf of its own: an earlier one may still be shown by an iterator.
GdkPixbuf *
Lottie::render(gint64 i)
{
	GdkPixbuf *pixbuf;
	uint32_t *pixels;
	int stride;
	tvg::Result r;

	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width_, height_);
	if (pixbuf == nullptr)
		return (nullptr);
	pixels = (uint32_t *)gdk_pixbuf_get_pixels(pixbuf);
	stride = gdk_pixbuf_get_rowstride(pixbuf) / 4;

	// GdkPixbuf wants R, G, B, A in memory, not premultiplied.  ThorVG
	// names its formats by 32-bit word, so on little-endian machines
	// that is ABGR; big-endian ones get ARGB, rotated below.
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	r = canvas_->target(pixels, stride, width_, height_,
	    tvg::ColorSpace::ABGR8888S);
#else
	r = canvas_->target(pixels, stride, width_, height_,
	    tvg::ColorSpace::ARGB8888S);
#endif
	if (r != tvg::Result::Success)
		goto fail;
	// ThorVG refuses to go to the frame it is already on; that is fine.
	r = anim_->frame((float)i);
	if (r != tvg::Result::Success &&
	    r != tvg::Result::InsufficientCondition)
		goto fail;
	if (canvas_->update() != tvg::Result::Success ||
	    canvas_->draw(true) != tvg::Result::Success ||
	    canvas_->sync() != tvg::Result::Success)
		goto fail;
#if G_BYTE_ORDER == G_BIG_ENDIAN
	for (int y = 0; y < height_; y++) {
		uint32_t *p = pixels + (gsize)y * stride;

		for (int x = 0; x < width_; x++)
			p[x] = p[x] << 8 | p[x] >> 24;
	}
#endif
	return (pixbuf);
fail:
	g_object_unref(pixbuf);
	return (nullptr);
}

} // namespace tvgpb
