#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Mikhail Teterin
#
# Write the sample files into data/.  Each is a 64x48 box filling the whole
# picture, its colour given per frame with hold keyframes, so that the test
# knows what every frame looks like.  The files are committed; there is no
# need to run this unless they change.
#
# usage: gen-data.sh [data-dir]

set -e
d=${1:-$(dirname "$0")/data}
mkdir -p "$d"

# lottie <frame rate> <frames> <opacity> <color keyframes>
lottie()
{
	printf '{"v":"5.7.0","fr":%s,"ip":0,"op":%s,"w":64,"h":48,' "$1" "$2"
	printf '"nm":"box","ddd":0,"assets":[],"layers":[{"ddd":0,"ind":1,'
	printf '"ty":4,"nm":"box","sr":1,"ks":{"o":{"a":0,"k":100},'
	printf '"r":{"a":0,"k":0},"p":{"a":0,"k":[32,24,0]},'
	printf '"a":{"a":0,"k":[0,0,0]},"s":{"a":0,"k":[100,100,100]}},"ao":0,'
	printf '"shapes":[{"ty":"gr","it":[{"ty":"rc","d":1,'
	printf '"s":{"a":0,"k":[64,48]},"p":{"a":0,"k":[0,0]},'
	printf '"r":{"a":0,"k":0}},{"ty":"fl","c":%s,' "$4"
	printf '"o":{"a":0,"k":%s},"r":1},{"ty":"tr","p":{"a":0,"k":[0,0]},' "$3"
	printf '"a":{"a":0,"k":[0,0]},"s":{"a":0,"k":[100,100]},'
	printf '"r":{"a":0,"k":0},"o":{"a":0,"k":100}}]}],'
	printf '"ip":0,"op":%s,"st":0,"bm":0}]}\n' "$2"
}

# 10 frames per second, 600 ms: red for 300 ms, green for 200, blue for 100.
rgb='{"a":1,"k":[{"t":0,"s":[1,0,0,1],"h":1},{"t":3,"s":[0,1,0,1],"h":1},'
rgb="$rgb"'{"t":5,"s":[0,0,1,1],"h":1}]}'

lottie 10 6 100 "$rgb" > "$d/rgb.json"
# The same, compressed as Telegram does its stickers.
gzip -9 -n < "$d/rgb.json" > "$d/rgb.tgs"
# One frame: a still picture.
lottie 10 1 100 '{"a":0,"k":[0,0,1,1]}' > "$d/still.json"
# Half-transparent red.
lottie 10 1 50 '{"a":0,"k":[1,0,0,1]}' > "$d/alpha.json"
# JSON, but not Lottie.
printf '{"layers":[],"name":"not an animation"}\n' > "$d/not-lottie.json"
