NEXMON_VERSION = master
NEXMON_SITE = $(call github,seemoo-lab,nexmon,$(NEXMON_VERSION))
NEXMON_LICENSE = GPL-3.0
NEXMON_LICENSE_FILES = LICENSE.txt

LD_VERSION   := $(subst ., ,$(lastword $(BR2_LINUX_KERNEL_VERSION)))
LD_MAJOR_VER := $(word 1,$(LD_VERSION))
LD_MINOR_VER := $(word 2,$(LD_VERSION))

NEXMON_MODULE_SUBDIRS = patches/driver/brcmfmac_$(LD_MAJOR_VER).$(LD_MINOR_VER).y-nexmon

$(eval $(kernel-module))
$(eval $(generic-package))
