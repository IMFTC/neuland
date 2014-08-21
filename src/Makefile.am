NULL =

bin_PROGRAMS = \
	neuland \
	$(NULL)

noinst_PROGRAMS = \
	test \
	$(NULL)

test_SOURCES = \
	test-utils.c \
	neuland-utils.c \
	neuland-utils.h \
	$(NULL)

test_CFLAGS = $(NEULAND_CFLAGS)
test_LDADD = $(NEULAND_LIBS)

BUILT_SOURCES = \
	neuland-resources.c \
	neuland-resources.h \
	neuland-enums.h \
	neuland-enums.c \
	$(NULL)

neuland_SOURCES = \
	neuland-application.c \
	neuland-utils.c \
	neuland-utils.h \
	neuland-contact-row.c \
	neuland-contact-row.h \
	neuland-contact.c \
	neuland-contact.h \
	neuland-tox.h \
	neuland-tox.c \
	neuland-window.c \
	neuland-window.h \
	neuland-chat-widget.c \
	neuland-chat-widget.h \
	neuland-add-dialog.c \
	neuland-add-dialog.h \
	neuland-me-popover.c \
	neuland-me-popover.h \
	$(NULL)

nodist_neuland_SOURCES = \
	$(BUILT_SOURCES)

neuland_CFLAGS = $(NEULAND_CFLAGS)
neuland_LDADD = $(NEULAND_LIBS)

EXTRA_DIST = \
	neuland-enums.c.template \
	neuland-enums.h.template \
	neuland.gresource.xml \
	$(resource_files) \
	$(NULL)

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""${datadir}/locale"\" \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES)\
	$(NULL)

enum_headers = \
	neuland-contact.h \
	$(NULL)

neuland-enums.h: neuland-enums.h.template $(enum_headers)
	$(AM_V_GEN) glib-mkenums --template $< $(enum_headers) > $@

neuland-enums.c: neuland-enums.c.template $(enum_headers)
	$(AM_V_GEN) glib-mkenums --template $< $(enum_headers) > $@

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) \
	 --generate-dependencies $(srcdir)/neuland.gresource.xml)

neuland-resources.c: neuland.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) \
	--generate-source --c-name neuland $(srcdir)/neuland.gresource.xml

neuland-resources.h: neuland.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) \
	 --generate-header --c-name neuland $(srcdir)/neuland.gresource.xml
