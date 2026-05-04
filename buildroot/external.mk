include $(sort $(wildcard $(BR2_EXTERNAL_VANILLA_PATH)/package/*/*.mk))

ifdef BR2_LINUX_TEGRA_T210_DEPS

LINUX_CFLAGS += -Wno-maybe-uninitialized -Wno-address-of-packed-member \
                -Wno-stringop-truncation -Wno-address \
                -Wno-array-bounds -Wno-stringop-overread

LINUX_MAKE_FLAGS += tegra-dtstree=../hardware/nvidia

NX_VER = linux-dev
NV_VER = linux-dev
NG_VER = linux-3.4.0-r32.5
DT_VER = l4t/l4t-r32.5

define LINUX_ACQUIRE_DEPS
	test -d "$(BUILD_DIR)/nvidia" || \
		git clone -b $(NV_VER) --single-branch \
		https://github.com/theofficialgman/switch-l4t-kernel-nvidia.git \
		"$(BUILD_DIR)/nvidia"
	test -d "$(BUILD_DIR)/hardware/nvidia/platform/t210/nx" || \
		git clone -b $(NX_VER) --single-branch \
		https://github.com/theofficialgman/switch-l4t-platform-t210-nx.git \
		"$(BUILD_DIR)/hardware/nvidia/platform/t210/nx"
	test -d "$(BUILD_DIR)/nvgpu" || \
		git clone -b $(NG_VER) --single-branch \
		https://gitlab.com/switchroot/kernel/l4t-kernel-nvgpu \
		"$(BUILD_DIR)/nvgpu"
	test -d "$(BUILD_DIR)/hardware/nvidia/soc/t210" || \
		git clone -b $(DT_VER) --single-branch \
		https://gitlab.com/switchroot/kernel/l4t-soc-t210 \
		"$(BUILD_DIR)/hardware/nvidia/soc/t210"
	test -d "$(BUILD_DIR)/hardware/nvidia/soc/tegra" || \
		git clone -b $(DT_VER) --single-branch \
		https://gitlab.com/switchroot/kernel/l4t-soc-tegra \
		"$(BUILD_DIR)/hardware/nvidia/soc/tegra/"
	test -d "$(BUILD_DIR)/hardware/nvidia/platform/tegra/common" || \
		git clone -b $(DT_VER) --single-branch \
		https://gitlab.com/switchroot/kernel/l4t-platform-tegra-common \
		"$(BUILD_DIR)/hardware/nvidia/platform/tegra/common/"
	test -d "$(BUILD_DIR)/hardware/nvidia/platform/t210/common" || \
		git clone -b $(DT_VER) --single-branch \
		https://gitlab.com/switchroot/kernel/l4t-platform-t210-common \
		"$(BUILD_DIR)/hardware/nvidia/platform/t210/common/"
endef
LINUX_POST_EXTRACT_HOOKS += LINUX_ACQUIRE_DEPS

endif # BR2_LINUX_TEGRA_T210_DEPS
