ifeq ($(BR2_PACKAGE_ONBOARD),y)
PYTHON_GOBJECT_CONF_OPTS := $(filter-out -Dpycairo=disabled,$(PYTHON_GOBJECT_CONF_OPTS)) -Dpycairo=enabled
PYTHON_GOBJECT_DEPENDENCIES += python-pycairo

$(PYTHON_GOBJECT_TARGET_CONFIGURE): | python-pycairo

# BR2_EXTERNAL is read after Buildroot has defined the package install rules.
# Stage PyCairo through its existing Meson install command before PyGObject.
define ONBOARD_STAGE_PYCAIRO
	$(PYTHON_PYCAIRO_INSTALL_STAGING_CMDS)
endef
PYTHON_PYCAIRO_POST_INSTALL_TARGET_HOOKS += ONBOARD_STAGE_PYCAIRO
endif
