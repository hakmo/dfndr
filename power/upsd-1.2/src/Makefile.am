pkgsysconfdir = $(sysconfdir)/$(PACKAGE)
pkglibexecdir = $(libexecdir)/$(PACKAGE)
pkglocalstatedir = $(localstatedir)/$(PACKAGE)

AM_CPPFLAGS = -D_GNU_SOURCE -DCONF_DIR=\"$(pkgsysconfdir)\" -DSCRIPT_DIR=\"$(pkglibexecdir)\" -DSTATE_DIR=\"$(pkglocalstatedir)\"
AM_CFLAGS = -std=c99 -Wall -Wextra
AM_LDFLAGS = -lrt

bin_PROGRAMS = upsd
upsd_SOURCES = common.c common.h logger.c logger.h main.c main.h settings.c settings.h upsd.c upsd.h 
pkgsysconf_DATA = upsd.conf
pkglibexec_SCRIPTS = low_battery power_back power_outage
man_MANS = upsd.1 upsd.conf.5

EXTRA_DIST = upsd.conf low_battery power_back power_outage upsd.1 upsd.conf.5 init.d/upsd

install-data-local:
	$(MKDIR_P) $(pkglocalstatedir)

uninstall-local:
	rm -rf $(pkgsysconfdir)
	rm -rf $(pkglibexecdir)
	rm -rf $(pkglocalstatedir)
