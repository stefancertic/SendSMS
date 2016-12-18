bin_PROGRAMS = sendsms

AM_CPPFLAGS = $(CURL_CFLAGS) $(GLIB_CFLAGS)
AM_CFLAGS = -Wall
if DEBUG
AM_CPPFLAGS += -g3 -O0 -DDEBUG
else
AM_CPPFLAGS += -O2
endif

sendsms_SOURCES = sendsms.c sendsms.h sms-api.c sms-api.h web.c web.h encoder.c encoder.h debug.h
sendsms_LDADD = $(CURL_LIBS) $(GLIB_LIBS)

