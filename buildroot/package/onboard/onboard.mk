################################################################################
#
# onboard
#
################################################################################

ONBOARD_VERSION = 1.4.4-6
ONBOARD_SITE = $(call github,onboard-osk,onboard,v$(ONBOARD_VERSION))
ONBOARD_LICENSE = GPL-3.0+, BSD-3-Clause, CC-BY-4.0 (emoji artwork)
ONBOARD_LICENSE_FILES = COPYING COPYING.GPL3 COPYING.BSD3
ONBOARD_SETUP_TYPE = setuptools
ONBOARD_INSTALL_TARGET_OPTS = --data=$(TARGET_DIR)/usr
ONBOARD_DEPENDENCIES = \
	host-librsvg \
	host-pkgconf \
	dbus-python \
	dejavu \
	gsettings-desktop-schemas \
	libgtk3 \
	librsvg \
	python-gobject \
	python-pycairo \
	udev \
	xlib_libXext \
	xlib_libxkbfile \
	xlib_libXtst

ifeq ($(BR2_PACKAGE_LIBGTK3_WAYLAND),y)
ONBOARD_DEPENDENCIES += wayland libxkbcommon
endif

define ONBOARD_INSTALL_DATA
	$(INSTALL) -D -m 0644 $(@D)/data/org.onboard.gschema.xml \
		$(STAGING_DIR)/usr/share/glib-2.0/schemas/org.onboard.gschema.xml
	$(INSTALL) -D -m 0644 $(ONBOARD_PKGDIR)/Vanilla.onboard \
		$(TARGET_DIR)/usr/share/onboard/layouts/Vanilla.onboard
	rm -rf $(TARGET_DIR)/usr/share/onboard/emojione
endef
ONBOARD_POST_INSTALL_TARGET_HOOKS += ONBOARD_INSTALL_DATA

$(eval $(python-package))
