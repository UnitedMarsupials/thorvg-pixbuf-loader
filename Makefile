# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Mikhail Teterin
#
# The portable part of the build: POSIX make, plus the "!=" assignment that
# both BSD make and GNU make (4.0 and later) understand.  BSDmakefile and
# GNUmakefile, which either make reads in preference to this file, include
# it and hold whatever is particular to one make or one system.
#
# A loader is only found in the directory gdk-pixbuf looks in, so that is
# where it is installed; DESTDIR stages it elsewhere.

PKG_CONFIG=	pkg-config
PACKAGES=	gdk-pixbuf-2.0 gmodule-2.0 thorvg-1

MODULEDIR!=	${PKG_CONFIG} --variable=gdk_pixbuf_moduledir gdk-pixbuf-2.0
QUERY_LOADERS!=	${PKG_CONFIG} --variable=gdk_pixbuf_query_loaders \
		    gdk-pixbuf-2.0
PKG_CFLAGS!=	${PKG_CONFIG} --cflags ${PACKAGES}
PKG_LIBS!=	${PKG_CONFIG} --libs ${PACKAGES}
TEST_CFLAGS!=	${PKG_CONFIG} --cflags gdk-pixbuf-2.0
TEST_LIBS!=	${PKG_CONFIG} --libs gdk-pixbuf-2.0

MODULE=		libpixbufloader-thorvg.so
OBJS=		src/io-lottie.o src/io-lottie-animation.o src/lottie.o
WARNS=		-Wall -Wextra
ALL_CXXFLAGS=	-std=c++14 -fPIC -fvisibility=hidden ${WARNS} ${PKG_CFLAGS} \
		${CXXFLAGS}
ALL_CFLAGS=	-std=c11 ${WARNS} ${TEST_CFLAGS} ${CFLAGS}

all: ${MODULE}

.SUFFIXES: .cc .o
.cc.o:
	${CXX} ${ALL_CXXFLAGS} -c -o $@ $<

${OBJS}: src/lottie.h
src/io-lottie.o src/io-lottie-animation.o: src/io-lottie-animation.h

${MODULE}: ${OBJS}
	${CXX} -shared -Wl,-z,defs ${LDFLAGS} -o $@ ${OBJS} ${PKG_LIBS}

# The tests load the sample files through a loaders cache that knows only
# the module just built.
tests/test-loader: tests/test-loader.c
	${CC} ${ALL_CFLAGS} ${LDFLAGS} -o $@ tests/test-loader.c ${TEST_LIBS}

tests/loaders.cache: ${MODULE}
	${QUERY_LOADERS} "$$(pwd)/${MODULE}" > $@

check test: tests/test-loader tests/loaders.cache
	GDK_PIXBUF_MODULE_FILE=tests/loaders.cache tests/test-loader tests/data

install: ${MODULE}
	mkdir -p ${DESTDIR}${MODULEDIR}
	install -m 0755 ${MODULE} ${DESTDIR}${MODULEDIR}
	test -n "${DESTDIR}" || ${QUERY_LOADERS} --update-cache

clean:
	rm -f ${OBJS} ${MODULE} tests/test-loader tests/loaders.cache

.PHONY: all check test install clean
