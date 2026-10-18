// SPDX-License-Identifier: BSD-2-Clause
//
// Copyright (c) 2026 Mikhail Teterin
//
// A Lottie animation, loaded into ThorVG, whose frames are rendered on
// demand.

#ifndef TVGPB_LOTTIE_H
#define TVGPB_LOTTIE_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <memory>
#include <mutex>
#include <thorvg.h>

#define TVGPB_MAX_DIMENSION 16384
#define TVGPB_MAX_PIXELS    (100ULL * 1000 * 1000)

namespace tvgpb {

class Lottie {
    public:
	// Load a whole file: Lottie JSON or, if ThorVG inflates it, gzipped
	// Lottie (Telegram's TGS).
	static Lottie *open(const guint8 *data, gsize len, GError **error);

	~Lottie();

	int width() const { return (width_); }
	int height() const { return (height_); }
	gint64 n_frames() const { return (n_frames_); }
	gint64 total_us() const { return (total_us_); }

	// When frame i starts, 0 <= i <= n_frames().
	gint64 start_us(gint64 i) const { return (i * total_us_ / n_frames_); }

	// Index of the frame shown at pos_us (0 <= pos_us < total_us()).
	gint64 frame_at(gint64 pos_us) const
	{
		return (pos_us * n_frames_ / total_us_);
	}

	// Render all frames at this size; call before the first frame is used.
	void set_size(int width, int height);

	// Frame 0, owned by the animation; NULL on failure.
	GdkPixbuf *first();

	// New reference to a frame, rendered on demand; NULL on failure.
	GdkPixbuf *frame(gint64 i);

    private:
	struct CanvasDeleter {
		void operator()(tvg::SwCanvas *c) const { delete c; }
	};
	struct AnimationDeleter {
		void operator()(tvg::Animation *a) const { delete a; }
	};

	Lottie() = default;
	GdkPixbuf *render(gint64 i);

	std::unique_ptr<tvg::Animation, AnimationDeleter> anim_;
	std::unique_ptr<tvg::SwCanvas, CanvasDeleter> canvas_;
	std::mutex mu_;
	GdkPixbuf *first_ = nullptr;
	GdkPixbuf *last_ = nullptr; // the most recently rendered frame
	gint64 last_index_ = -1;
	float natural_w_ = 0, natural_h_ = 0;
	int width_ = 0, height_ = 0;
	gint64 n_frames_ = 1, total_us_ = 0;
};

} // namespace tvgpb

#endif
