JOYCOND_VERSION = master
JOYCOND_SITE = $(call github,DanielOgorchock,joycond,$(JOYCOND_VERSION))
JOYCOND_LICENSE = GPL-3.0
JOYCOND_LICENSE_FILES = LICENSE

$(eval $(cmake-package))
