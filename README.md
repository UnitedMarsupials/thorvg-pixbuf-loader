# thorvg-pixbuf-loader

## Goal

Lottie is the vector animation format of stickers, emoji and small UI
animations: JSON describing shapes and their keyframes, or, as Telegram's
animated stickers (`.tgs`), the same JSON gzipped.  GTK applications get
their pictures through
[gdk-pixbuf](https://gitlab.gnome.org/GNOME/gdk-pixbuf), which knows nothing
about it.  This project is a gdk-pixbuf loader plugin that renders Lottie
with [ThorVG](https://www.thorvg.org/): any program that can show a PNG or a
GIF through gdk-pixbuf (Pidgin, image viewers, file managers, ...) can show
a Lottie file as an image or as an animation.

The immediate use is Pidgin, through tdlib-purple, showing Telegram's
animated stickers.

## What it does

* A one-frame file is a still picture; anything longer is an animation that
  loops forever.  `gdk_pixbuf_new_from_file()` returns the first frame.
* Being vector graphics, it is rendered at the size the application asks
  for (`gdk_pixbuf_new_from_file_at_size()` and friends), not scaled from a
  bitmap.
* Frames are rendered on demand, so a thumbnail costs one frame.
* RGBA output, not premultiplied, as gdk-pixbuf wants it.
* gzipped Lottie (TGS) is inflated by ThorVG, if it can; ThorVG 1.1.3 as
  released cannot.
* Input is untrusted: sizes are capped (16384 per side, 100 Mpixel, 64 MiB
  file), and image assets that are not embedded in the file are never
  opened.

gdk-pixbuf gives a file to the one loader that registers the content type
GIO guesses for it.  shared-mime-info knows no Lottie type, so besides
`video/lottie+json` and `application/x-tgsticker` the loader registers what
GIO calls Lottie and TGS files and data: `application/json`, `text/plain`
(JSON without a file name) and `application/gzip`.  What is not Lottie is
refused, as no other loader would have taken it either.

Not supported: dotLottie (`.lottie`, a ZIP archive), which ThorVG cannot
read; SVG, which is left to librsvg.

## Building

Needs gdk-pixbuf 2.36+, ThorVG 1.x and pkg-config, and BSD make or GNU make
4.0+.

    make
    make test
    make install          # also refreshes the loaders cache

The loader goes where the installed gdk-pixbuf looks for loaders; use
`make PREFIX=... install` for another prefix and `DESTDIR` for staging.
The tests load the files in `tests/data` through a private loaders cache;
`tests/gen-data.sh` regenerates them.

## License

BSD 2-clause; see [LICENSE](LICENSE).  Developers: please read
[AGENTS.md](AGENTS.md) first.  The code follows FreeBSD's
[style(9)](https://man.freebsd.org/cgi/man.cgi?query=style&sektion=9).
