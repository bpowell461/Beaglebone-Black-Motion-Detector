# Include the detector package
ifeq ($(BR2_PACKAGE_DETECTOR),y)
PACKAGES += detector
endif

# Define the directory for the detector package
detector_DIR = $(BR2_EXTERNAL)/package/detector
