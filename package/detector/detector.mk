DETECTOR_VERSION = 1.0
DETECTOR_SITE = $(TOPDIR)/../..
DETECTOR_SITE_METHOD = local

DETECTOR_DEPENDENCIES = 

define DETECTOR_BUILD_CMDS
    $(MAKE) -C $(@D)
endef

define DETECTOR_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/detector $(TARGET_DIR)/usr/bin/detector
endef

$(eval $(generic-package))