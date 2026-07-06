NEXMON_VERSION = master
NEXMON_SITE = $(call github,vanilla-wiiu,nexmon,$(NEXMON_VERSION))
NEXMON_LICENSE = GPL-3.0
NEXMON_LICENSE_FILES = LICENSE.txt

define NEXMON_BUILD_CMDS
	$(TARGET_MAKE_ENV)
		. $(@D)/setup_env.sh && \
		$(MAKE1) -C $(@D)
endef

define NEXMON_INSTALL_TARGET_CMDS
	$(TARGET_MAKE_ENV) \
		. $(@D)/setup_env.sh && \
		$(MAKE1) -C $(@D)/patches/bcm4356/7_35_101_5_sta/wiiu
    cp $(@D)/patches/bcm4356/7_35_101_5_sta/wiiu/brcmfmac4356-pcie.bin \
        $(TARGET_DIR)/lib/firmware/brcm/brcmfmac4356A3-pcie.bin
endef

$(eval $(kernel-module))
$(eval $(generic-package))
