if GUI
bin_PROGRAMS = gsendsms

AM_CPPFLAGS = $(GTK_CFLAGS)
AM_CFLAGS = -Wall -I../src -export-dynamic

if DEBUG
AM_CFLAGS += -g3 -O0 -DDEBUG
else
AM_CFLAGS += -O2
endif

gsendsms_SOURCES = gsendsms.c ../src/encoder.c ../src/encoder.h
gsendsms_LDADD = $(GTK_LIBS)
gsendsmsdir = $(datadir)/sendsms
dist_gsendsms_DATA = gsendsms.glade

endif

