PREFIX?=/usr/local
BINDIR=${PREFIX}/bin

PROG=volo
NOMAN=

#DEBUG= -g

SRCS= gtk.h webkit.h uri_entry.cpp uri_entry.h volo.cpp volo.h
CXXFLAGS+= -Wall -std=c++1y -I. -I${.CURDIR}
CXXFLAGS+= -fno-rtti -fno-exceptions
LDADD= -lutil
LIBS+= gtk+-3.0 webkit2gtk-3.0
LIBS_CXXFLAGS!= pkg-config --cflags $(LIBS)
LIBS_LDFLAGS!= pkg-config --libs $(LIBS)
CXXFLAGS+= $(LIBS_CXXFLAGS)
LDFLAGS+= $(LIBS_LDFLAGS)

MANDIR= ${PREFIX}/man/man

beforeinstall:
	install -m 755 -d ${PREFIX}/bin
	#install -m 755 -d ${PREFIX}/man/man1/
	#install -m 755 -d ${PREFIX}/share/volo
	#install -m 755 -d ${PREFIX}/share/applications
	#install -m 644 ${.CURDIR}/volo.desktop ${PREFIX}/share/applications

.include <bsd.prog.mk>
