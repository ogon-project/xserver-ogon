bin_PROGRAMS = ogon-backend-x
noinst_LIBRARIES = libfbcmap.a

AM_CFLAGS = -DHAVE_DIX_CONFIG_H \
	$(XOGONMODULES_CFLAGS) \
	$(DIX_CFLAGS) @XORG_CFLAGS@

SRCS=	 \
	$(top_srcdir)/mi/miinitext.c \
	rdpInput.c \
	rdpMain.c \
	rdpMisc.c \
	rdpModes.c \
	rdpRandr.c \
	rdpScreen.c \
	rdpUpdate.c \
	rdpMultitouch.c

libfbcmap_a_SOURCES = $(top_srcdir)/fb/fbcmap_mi.c
libfbcmap_a_CFLAGS = $(AM_CFLAGS)

ogon_backend_x_SOURCES = $(SRCS)

XOGON_LIBS = \
	@XOGON_LIBS@ \
	libfbcmap.a

ogon_backend_x_DEPENDENCIES = $(XOGON_LIBS)
ogon_backend_x_LDADD = $(XOGON_LIBS) $(XOGON_SYS_LIBS) $(XSERVER_SYS_LIBS)
ogon_backend_x_LDFLAGS = $(LD_EXPORT_SYMBOLS_FLAG)

relink:
	$(AM_V_at)rm -f ogon-backend-x$(EXEEXT) && $(MAKE) ogon-backend-x$(EXEEXT)
