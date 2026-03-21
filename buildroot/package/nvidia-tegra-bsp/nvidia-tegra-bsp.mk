NVIDIA_TEGRA_BSP_VERSION = 32.7.6
NVIDIA_TEGRA_BSP_DEBS = $(BR2_EXTERNAL_VANILLA_PATH)/package/nvidia-tegra-bsp/debs
NVIDIA_TEGRA_BSP_SITE = $(NVIDIA_TEGRA_BSP_DEBS)
NVIDIA_TEGRA_BSP_SITE_METHOD = local

#NVIDIA_TEGRA_BSP_PROVIDES = libegl libgles

TEGRA_TEGRA_DIR = /usr/lib/aarch64-linux-gnu/tegra

define NVIDIA_TEGRA_BSP_EXTRACT_DEB
    cd $(TARGET_DIR) && ar p $(strip $(1)) data.tar.zst | tar --zstd -xf -
endef

define NVIDIA_TEGRA_BSP_INSTALL_TARGET_CMDS
    $(call NVIDIA_TEGRA_BSP_EXTRACT_DEB,$(wildcard $(NVIDIA_TEGRA_BSP_DEBS)/nvidia-l4t-core_*.deb))
    $(call NVIDIA_TEGRA_BSP_EXTRACT_DEB,$(wildcard $(NVIDIA_TEGRA_BSP_DEBS)/nvidia-l4t-3d-core_*.deb))
    $(call NVIDIA_TEGRA_BSP_EXTRACT_DEB,$(wildcard $(NVIDIA_TEGRA_BSP_DEBS)/nvidia-l4t-x11_*.deb))
    $(call NVIDIA_TEGRA_BSP_EXTRACT_DEB,$(wildcard $(NVIDIA_TEGRA_BSP_DEBS)/nvidia-l4t-multimedia_*.deb))
    $(call NVIDIA_TEGRA_BSP_EXTRACT_DEB,$(wildcard $(NVIDIA_TEGRA_BSP_DEBS)/nvidia-l4t-xusb-firmware_*.deb))
    $(call NVIDIA_TEGRA_BSP_EXTRACT_DEB,$(wildcard $(NVIDIA_TEGRA_BSP_DEBS)/nvidia-l4t-firmware_*.deb))

    # Symlink tegra libs into standard lib path
    for lib in $(TARGET_DIR)$(TEGRA_TEGRA_DIR)/*.so*; do \
        ln -sf $(TEGRA_TEGRA_DIR)/$$(basename $$lib) \
            $(TARGET_DIR)/usr/lib/aarch64-linux-gnu/$$(basename $$lib); \
    done

    # GM20B firmware symlinks expected by the GPU driver
    $(INSTALL) -d $(TARGET_DIR)/lib/firmware/gm20b
    ln -sf ../tegra21x/nv_acr_ucode_prod.bin \
        $(TARGET_DIR)/lib/firmware/gm20b/acr_ucode.bin

    # A .deb may contain this but Buildroot will fail if it's present
    rm -r $(TARGET_DIR)/etc/ld.so.conf.d
endef

define NVIDIA_TEGRA_BSP_INSTALL_STAGING_CMDS
    # EGL/GLES headers
    if [ -d $(TARGET_DIR)/usr/include/EGL ]; then \
        cp -a $(TARGET_DIR)/usr/include/EGL \
            $(STAGING_DIR)/usr/include/; \
    fi
    if [ -d $(TARGET_DIR)/usr/include/GLES2 ]; then \
        cp -a $(TARGET_DIR)/usr/include/GLES2 \
            $(STAGING_DIR)/usr/include/; \
    fi

    # EGL/GLES libs for linking
    cp -a $(TARGET_DIR)$(TEGRA_TEGRA_DIR)/libEGL* $(STAGING_DIR)/usr/lib/
    cp -a $(TARGET_DIR)$(TEGRA_TEGRA_DIR)/libGLES* $(STAGING_DIR)/usr/lib/

    # pkg-config files
    $(INSTALL) -d $(STAGING_DIR)/usr/lib/pkgconfig/
    printf 'Name: egl\nDescription: NVIDIA Tegra EGL\nVersion: 1.5\nLibs: -lEGL\nCflags:\n' \
        > $(STAGING_DIR)/usr/lib/pkgconfig/egl.pc
    printf 'Name: glesv2\nDescription: NVIDIA Tegra GLES2\nVersion: 3.2\nLibs: -lGLESv2\nCflags:\n' \
        > $(STAGING_DIR)/usr/lib/pkgconfig/glesv2.pc
endef

$(eval $(generic-package))
