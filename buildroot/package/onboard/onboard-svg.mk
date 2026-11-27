ifeq ($(BR2_PACKAGE_ONBOARD),y)
# Onboard needs GdkPixbuf to load certain SVG icons
LIBRSVG_CONF_OPTS := $(filter-out --disable-pixbuf-loader,$(LIBRSVG_CONF_OPTS)) --enable-pixbuf-loader

# pkg-config's sysroot prefix may contain bin/../ - keep only the /usr path
define ONBOARD_FIX_RSVG_LOADER_PATHS
	$(SED) '/^gdk_pixbuf_.* = /s, = .*/usr/, = /usr/,' \
		$(@D)/gdk-pixbuf-loader/Makefile
	# Relink any existing loader with the corrected install directory.
	rm -f $(@D)/gdk-pixbuf-loader/libpixbufloader-svg.la
endef
LIBRSVG_POST_CONFIGURE_HOOKS += ONBOARD_FIX_RSVG_LOADER_PATHS

# Query matching host loaders after all loaders have been installed
TARGET_FINALIZE_HOOKS += GDK_PIXBUF_UPDATE_CACHE
endif
