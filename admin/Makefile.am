## Cherokee: Makefile.am  -*- makefile -*-

SUBDIRS =

if INSTALL_ADMIN
SUBDIRS += static
endif

SUFFIXES = .py.pre .py

.py.pre.py:
	sed -e "s|%sysconfdir%|${sysconfdir}|g; s|%sbindir%|${sbindir}|g; s|%docdir%|${docdir}|g; s|%prefix%|${prefix}|g; s|%localstatedir%|${localstatedir}|g; s|%libdir%|${libdir}|g; s|%wwwroot%|${WWW_ROOT}|g; s|%version%|${PACKAGE_VERSION}|g; s|%phpcgi%|${PHPCGI}|g; s|%datadir%|${datadir}|g; s|%localedir%|${localedir}|g" $< > $@

PY_PRE = \
configured.py.pre


if INSTALL_ADMIN
gen_data = $(PY_PRE:.py.pre=.py)
endif

generateddir = "$(datadir)/cherokee/admin"
generated_DATA = $(gen_data)


if INSTALL_ADMIN
admin_scripts = server.py upgrade_config.py
endif

adminpydir = "$(datadir)/cherokee/admin"
adminpy_SCRIPTS = $(admin_scripts)


if INSTALL_ADMIN
admin_data = \
theme.html \
server.py \
consts.py \
util.py \
validations.py \
config_version.py \
Cherokee.py \
Page.py \
PageIndex.py \
PageGeneral.py \
PageVServer.py \
PageIcons.py \
PageMime.py \
PageAdvanced.py \
plugins/rrd.py \
plugins/post_track.py \
plugins/evhost.py
endif

adminpy_DATA = $(admin_data)

if INSTALL_ADMIN
template_data =
endif

adminhelpdir = "$(datadir)/cherokee/admin"
adminhelp_DATA = $(template_data)

EXTRA_DIST = \
$(PY_PRE) \
$(admin_data) \
$(admin_scripts) \
$(template_data)

CLEANFILES = \
$(generated_DATA)

