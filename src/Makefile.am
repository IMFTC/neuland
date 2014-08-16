NULL =

bin_PROGRAMS = \
	neuland \
	test \
	$(NULL)

test_SOURCES = \
	test-utils.c \
	neuland-utils.c \
	neuland-utils.h \
	$(NULL)

test_CFLAGS = $(NEULAND_CFLAGS)
test_LDADD = $(NEULAND_LIBS)

neuland_SOURCES = \
	neuland-application.c \
	neuland-utils.c \
	neuland-utils.h \
	neuland-contact-row.c \
	neuland-contact-row.h \
	neuland-contact.c \
	neuland-contact.h \
	neuland-enums.c \
	neuland-enums.h \
	neuland-tox.h \
	neuland-tox.c \
	neuland-window.c \
	neuland-window.h \
	neuland-chat-widget.c \
	neuland-chat-widget.h \
	neuland-add-dialog.c \
	neuland-add-dialog.h \
	$(NULL)

nodist_neuland_SOURCES = \
	neuland-resources.c \
	neuland-resources.h \
	$(NULL)

BUILT_SOURCES = \
	neuland-resources.c \
	neuland-resources.h \
	$(NULL)

EXTRA_DIST = \
	m4/.empty \
	neuland.gresource.xml \
	$(resource_files) \
	$(NULL)

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""${datadir}/locale"\" \
	$(NULL)

neuland_CFLAGS = $(NEULAND_CFLAGS)
neuland_LDADD = $(NEULAND_LIBS)

resource_files=$(shell $(GLIB_COMPILE_RESOURCES) \--sourcedir=$(srcdir) \
	 --generate-dependencies $(srcdir)/neuland.gresource.xml)

neuland-resources.c: neuland.gresource.xml $(resource_files)
	$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) \
	--generate-source --c-name neuland $(srcdir)/neuland.gresource.xml

neuland-resources.h: neuland.gresource.xml $(resource_files)
	$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) \
	 --generate-header --c-name neuland $(srcdir)/neuland.gresource.xml
