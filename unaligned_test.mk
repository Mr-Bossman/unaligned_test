################################################################################
#
# UNALIGNED_TEST
#
################################################################################

UNALIGNED_TEST_VERSION = 1.0
UNALIGNED_TEST_SITE = $(UNALIGNED_TEST_PKGDIR)/src
UNALIGNED_TEST_SITE_METHOD = local

define UNALIGNED_TEST_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D) all
endef

define UNALIGNED_TEST_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/unaligned $(TARGET_DIR)/usr/bin
	$(INSTALL) -D -m 0755 $(@D)/unaligned_c $(TARGET_DIR)/usr/bin
	$(INSTALL) -D -m 0755 $(@D)/unaligned_vec $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
