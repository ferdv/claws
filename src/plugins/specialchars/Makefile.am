# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

plugindir = $(pkglibdir)/plugins

if BUILD_SPECIALCHARS_PLUGIN
plugin_LTLIBRARIES = specialchars.la
endif

specialchars_la_SOURCES = defs.h sc_utils.h \
			  sdfa.c sdfa.h \
			  subst.c subst.h \
			  specialchars.c

specialchars_la_LDFLAGS = \
	-avoid-version -module

if CYGWIN
cygwin_export_lib = -L$(top_builddir)/src -lclaws-mail
else
cygwin_export_lib = 
endif

specialchars_la_LIBADD = $(cygwin_export_lib) \
	$(GTK_LIBS) 

IFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
	-I$(top_builddir)/src/common \
	-I$(top_srcdir)/src/gtk

specialchars_la_CPPFLAGS = \
	$(IFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) 
