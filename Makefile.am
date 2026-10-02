# GPL (C) 1998 Andreas Tille
#   tille@debian.org
bin_PROGRAMS = xteddy

xteddy_SOURCES = xteddy.c
man_MANS = xteddy.6
bin_SCRIPTS = xtoys xteddy_test

AM_CFLAGS  = @IMLIB2_CFLAGS@
AM_LDFLAGS = @IMLIB2_LIBS@ -lX11

DEFS = -DPIXMAP_PATH=\"@PIXMAP_PATH@\" -Wall

SUBDIRS = html images
EXTRA_DIST = $(man_MANS)  $(bin_SCRIPTS) \
             README xteddy.README images.credit \
	     configure autogen.sh              
 


