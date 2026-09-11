################################################################################
#
# nx-ffmpeg
#
################################################################################

NX_FFMPEG_VERSION = 7.1.2-nvv4l2-drm
NX_FFMPEG_SITE = $(call gitlab,vanilla-wiiu,FFmpeg,$(NX_FFMPEG_VERSION))
NX_FFMPEG_INSTALL_STAGING = YES

NX_FFMPEG_LICENSE = LGPL-2.1+, libjpeg license
NX_FFMPEG_LICENSE_FILES = LICENSE.md COPYING.LGPLv2.1
ifeq ($(BR2_PACKAGE_NX_FFMPEG_GPL),y)
NX_FFMPEG_LICENSE += and GPL-2.0+
NX_FFMPEG_LICENSE_FILES += COPYING.GPLv2
endif

NX_FFMPEG_CONF_OPTS = \
	--prefix=/usr \
	--enable-avfilter \
	--disable-version3 \
	--enable-logging \
	--enable-optimizations \
	--disable-extra-warnings \
	--enable-avdevice \
	--enable-avcodec \
	--enable-avformat \
	--enable-network \
	--disable-gray \
	--enable-swscale-alpha \
	--disable-small \
	--disable-dxva2 \
	--enable-runtime-cpudetect \
	--disable-hardcoded-tables \
	--disable-mipsdsp \
	--disable-mipsdspr2 \
	--disable-msa \
	--enable-hwaccels \
	--disable-cuda \
	--disable-cuvid \
	--disable-nvenc \
	--disable-avisynth \
	--disable-frei0r \
	--disable-libopencore-amrnb \
	--disable-libopencore-amrwb \
	--disable-libdc1394 \
	--disable-libgsm \
	--disable-libilbc \
	--disable-libvo-amrwbenc \
	--disable-symver \
	--disable-doc

NX_FFMPEG_DEPENDENCIES += host-pkgconf

ifeq ($(BR2_PACKAGE_NX_FFMPEG_GPL),y)
NX_FFMPEG_CONF_OPTS += --enable-gpl
else
NX_FFMPEG_CONF_OPTS += --disable-gpl
endif

ifeq ($(BR2_PACKAGE_NX_FFMPEG_NONFREE),y)
NX_FFMPEG_CONF_OPTS += --enable-nonfree
else
NX_FFMPEG_CONF_OPTS += --disable-nonfree
endif

ifeq ($(BR2_PACKAGE_NX_FFMPEG_FFMPEG),y)
NX_FFMPEG_CONF_OPTS += --enable-ffmpeg
else
NX_FFMPEG_CONF_OPTS += --disable-ffmpeg
endif

ifeq ($(BR2_PACKAGE_NX_FFMPEG_FFPLAY),y)
NX_FFMPEG_DEPENDENCIES += sdl2
NX_FFMPEG_CONF_OPTS += --enable-ffplay
NX_FFMPEG_CONF_ENV += SDL_CONFIG=$(STAGING_DIR)/usr/bin/sdl2-config
else
NX_FFMPEG_CONF_OPTS += --disable-ffplay
endif

ifeq ($(BR2_PACKAGE_JACK1),y)
NX_FFMPEG_CONF_OPTS += --enable-libjack
NX_FFMPEG_DEPENDENCIES += jack1
else ifeq ($(BR2_PACKAGE_JACK2),y)
NX_FFMPEG_CONF_OPTS += --enable-libjack
NX_FFMPEG_DEPENDENCIES += jack2
else
NX_FFMPEG_CONF_OPTS += --disable-libjack
endif

ifeq ($(BR2_PACKAGE_LIBV4L),y)
NX_FFMPEG_DEPENDENCIES += libv4l
NX_FFMPEG_CONF_OPTS += --enable-libv4l2
else
NX_FFMPEG_CONF_OPTS += --disable-libv4l2
endif

ifeq ($(BR2_PACKAGE_NX_FFMPEG_FFPROBE),y)
NX_FFMPEG_CONF_OPTS += --enable-ffprobe
else
NX_FFMPEG_CONF_OPTS += --disable-ffprobe
endif

ifeq ($(BR2_PACKAGE_NX_FFMPEG_XCBGRAB),y)
NX_FFMPEG_CONF_OPTS += \
	--enable-libxcb \
	--enable-libxcb-shape \
	--enable-libxcb-shm \
	--enable-libxcb-xfixes
NX_FFMPEG_DEPENDENCIES += libxcb
else
NX_FFMPEG_CONF_OPTS += --disable-libxcb
endif

ifeq ($(BR2_PACKAGE_NX_FFMPEG_POSTPROC),y)
NX_FFMPEG_CONF_OPTS += --enable-postproc
else
NX_FFMPEG_CONF_OPTS += --disable-postproc
endif

ifeq ($(BR2_PACKAGE_NX_FFMPEG_SWSCALE),y)
NX_FFMPEG_CONF_OPTS += --enable-swscale
else
NX_FFMPEG_CONF_OPTS += --disable-swscale
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_ENCODERS)),all)
NX_FFMPEG_CONF_OPTS += --disable-encoders \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_ENCODERS)),--enable-encoder=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_DECODERS)),all)
NX_FFMPEG_CONF_OPTS += --disable-decoders \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_DECODERS)),--enable-decoder=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_MUXERS)),all)
NX_FFMPEG_CONF_OPTS += --disable-muxers \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_MUXERS)),--enable-muxer=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_DEMUXERS)),all)
NX_FFMPEG_CONF_OPTS += --disable-demuxers \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_DEMUXERS)),--enable-demuxer=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_PARSERS)),all)
NX_FFMPEG_CONF_OPTS += --disable-parsers \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_PARSERS)),--enable-parser=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_BSFS)),all)
NX_FFMPEG_CONF_OPTS += --disable-bsfs \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_BSFS)),--enable-bsf=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_PROTOCOLS)),all)
NX_FFMPEG_CONF_OPTS += --disable-protocols \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_PROTOCOLS)),--enable-protocol=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_FILTERS)),all)
NX_FFMPEG_CONF_OPTS += --disable-filters \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_FILTERS)),--enable-filter=$(x))
endif

ifeq ($(BR2_PACKAGE_NX_FFMPEG_INDEVS),y)
NX_FFMPEG_CONF_OPTS += --enable-indevs
ifeq ($(BR2_PACKAGE_ALSA_LIB),y)
NX_FFMPEG_CONF_OPTS += --enable-alsa
NX_FFMPEG_DEPENDENCIES += alsa-lib
else
NX_FFMPEG_CONF_OPTS += --disable-alsa
endif
else
NX_FFMPEG_CONF_OPTS += --disable-indevs
endif

ifeq ($(BR2_PACKAGE_NX_FFMPEG_OUTDEVS),y)
NX_FFMPEG_CONF_OPTS += --enable-outdevs
ifeq ($(BR2_PACKAGE_ALSA_LIB),y)
NX_FFMPEG_DEPENDENCIES += alsa-lib
endif
else
NX_FFMPEG_CONF_OPTS += --disable-outdevs
endif

ifeq ($(BR2_TOOLCHAIN_HAS_THREADS),y)
NX_FFMPEG_CONF_OPTS += --enable-pthreads
else
NX_FFMPEG_CONF_OPTS += --disable-pthreads
endif

ifeq ($(BR2_PACKAGE_ZLIB),y)
NX_FFMPEG_CONF_OPTS += --enable-zlib
NX_FFMPEG_DEPENDENCIES += zlib
else
NX_FFMPEG_CONF_OPTS += --disable-zlib
endif

ifeq ($(BR2_PACKAGE_BZIP2),y)
NX_FFMPEG_CONF_OPTS += --enable-bzlib
NX_FFMPEG_DEPENDENCIES += bzip2
else
NX_FFMPEG_CONF_OPTS += --disable-bzlib
endif

ifeq ($(BR2_PACKAGE_FDK_AAC)$(BR2_PACKAGE_NX_FFMPEG_NONFREE),yy)
NX_FFMPEG_CONF_OPTS += --enable-libfdk-aac
NX_FFMPEG_DEPENDENCIES += fdk-aac
else
NX_FFMPEG_CONF_OPTS += --disable-libfdk-aac
endif

ifeq ($(BR2_PACKAGE_NX_FFMPEG_GPL)$(BR2_PACKAGE_LIBCDIO_PARANOIA),yy)
NX_FFMPEG_CONF_OPTS += --enable-libcdio
NX_FFMPEG_DEPENDENCIES += libcdio-paranoia
else
NX_FFMPEG_CONF_OPTS += --disable-libcdio
endif

ifeq ($(BR2_PACKAGE_GNUTLS),y)
NX_FFMPEG_CONF_OPTS += --enable-gnutls --disable-openssl
NX_FFMPEG_DEPENDENCIES += gnutls
else
NX_FFMPEG_CONF_OPTS += --disable-gnutls
ifeq ($(BR2_PACKAGE_OPENSSL),y)
# openssl isn't license compatible with GPL
ifeq ($(BR2_PACKAGE_NX_FFMPEG_GPL)x$(BR2_PACKAGE_NX_FFMPEG_NONFREE),yx)
NX_FFMPEG_CONF_OPTS += --disable-openssl
else
NX_FFMPEG_CONF_OPTS += --enable-openssl
NX_FFMPEG_DEPENDENCIES += openssl
endif
else
NX_FFMPEG_CONF_OPTS += --disable-openssl
endif
endif

ifeq ($(BR2_PACKAGE_LIBDRM),y)
NX_FFMPEG_CONF_OPTS += --enable-libdrm
NX_FFMPEG_DEPENDENCIES += libdrm
else
NX_FFMPEG_CONF_OPTS += --disable-libdrm
endif

ifeq ($(BR2_PACKAGE_LIBOPENH264),y)
NX_FFMPEG_CONF_OPTS += --enable-libopenh264
NX_FFMPEG_DEPENDENCIES += libopenh264
else
NX_FFMPEG_CONF_OPTS += --disable-libopenh264
endif

ifeq ($(BR2_PACKAGE_LIBVORBIS),y)
NX_FFMPEG_DEPENDENCIES += libvorbis
NX_FFMPEG_CONF_OPTS += \
	--enable-libvorbis \
	--enable-muxer=ogg \
	--enable-encoder=libvorbis
endif

ifeq ($(BR2_PACKAGE_LIBVA),y)
NX_FFMPEG_CONF_OPTS += --enable-vaapi
NX_FFMPEG_DEPENDENCIES += libva
else
NX_FFMPEG_CONF_OPTS += --disable-vaapi
endif

ifeq ($(BR2_PACKAGE_LIBVDPAU),y)
NX_FFMPEG_CONF_OPTS += --enable-vdpau
NX_FFMPEG_DEPENDENCIES += libvdpau
else
NX_FFMPEG_CONF_OPTS += --disable-vdpau
endif

ifeq ($(BR2_PACKAGE_RPI_USERLAND),y)
NX_FFMPEG_CONF_OPTS += --enable-omx --enable-omx-rpi \
	--extra-cflags=-I$(STAGING_DIR)/usr/include/IL
NX_FFMPEG_DEPENDENCIES += rpi-userland
ifeq ($(BR2_arm),y)
NX_FFMPEG_CONF_OPTS += --enable-mmal
else
NX_FFMPEG_CONF_OPTS += --disable-mmal
endif
else
NX_FFMPEG_CONF_OPTS += --disable-mmal --disable-omx --disable-omx-rpi
endif

# To avoid a circular dependency only use opencv if opencv itself does
# not depend on ffmpeg.
ifeq ($(BR2_PACKAGE_OPENCV3_LIB_IMGPROC)x$(BR2_PACKAGE_OPENCV3_WITH_FFMPEG),yx)
NX_FFMPEG_CONF_OPTS += --enable-libopencv
NX_FFMPEG_DEPENDENCIES += opencv3
else
NX_FFMPEG_CONF_OPTS += --disable-libopencv
endif

ifeq ($(BR2_PACKAGE_OPUS),y)
NX_FFMPEG_CONF_OPTS += --enable-libopus
NX_FFMPEG_DEPENDENCIES += opus
else
NX_FFMPEG_CONF_OPTS += --disable-libopus
endif

ifeq ($(BR2_PACKAGE_LIBVPX),y)
NX_FFMPEG_CONF_OPTS += --enable-libvpx
NX_FFMPEG_DEPENDENCIES += libvpx
else
NX_FFMPEG_CONF_OPTS += --disable-libvpx
endif

ifeq ($(BR2_PACKAGE_LIBASS),y)
NX_FFMPEG_CONF_OPTS += --enable-libass
NX_FFMPEG_DEPENDENCIES += libass
else
NX_FFMPEG_CONF_OPTS += --disable-libass
endif

ifeq ($(BR2_PACKAGE_LIBBLURAY),y)
NX_FFMPEG_CONF_OPTS += --enable-libbluray
NX_FFMPEG_DEPENDENCIES += libbluray
else
NX_FFMPEG_CONF_OPTS += --disable-libbluray
endif

ifeq ($(BR2_PACKAGE_RTMPDUMP),y)
NX_FFMPEG_CONF_OPTS += --enable-librtmp
NX_FFMPEG_DEPENDENCIES += rtmpdump
else
NX_FFMPEG_CONF_OPTS += --disable-librtmp
endif

ifeq ($(BR2_PACKAGE_LAME),y)
NX_FFMPEG_CONF_OPTS += --enable-libmp3lame
NX_FFMPEG_DEPENDENCIES += lame
else
NX_FFMPEG_CONF_OPTS += --disable-libmp3lame
endif

ifeq ($(BR2_PACKAGE_LIBMODPLUG),y)
NX_FFMPEG_CONF_OPTS += --enable-libmodplug
NX_FFMPEG_DEPENDENCIES += libmodplug
else
NX_FFMPEG_CONF_OPTS += --disable-libmodplug
endif

ifeq ($(BR2_PACKAGE_LIBOPENMPT),y)
NX_FFMPEG_CONF_OPTS += --enable-libopenmpt
NX_FFMPEG_DEPENDENCIES += libopenmpt
else
NX_FFMPEG_CONF_OPTS += --disable-libopenmpt
endif

ifeq ($(BR2_PACKAGE_SPEEX),y)
NX_FFMPEG_CONF_OPTS += --enable-libspeex
NX_FFMPEG_DEPENDENCIES += speex
else
NX_FFMPEG_CONF_OPTS += --disable-libspeex
endif

ifeq ($(BR2_PACKAGE_LIBTHEORA),y)
NX_FFMPEG_CONF_OPTS += --enable-libtheora
NX_FFMPEG_DEPENDENCIES += libtheora
else
NX_FFMPEG_CONF_OPTS += --disable-libtheora
endif

ifeq ($(BR2_PACKAGE_LIBICONV),y)
NX_FFMPEG_CONF_OPTS += --enable-iconv
NX_FFMPEG_DEPENDENCIES += libiconv
else
NX_FFMPEG_CONF_OPTS += --disable-iconv
endif

ifeq ($(BR2_PACKAGE_LIBXML2),y)
NX_FFMPEG_CONF_OPTS += --enable-libxml2
NX_FFMPEG_DEPENDENCIES += libxml2
else
NX_FFMPEG_CONF_OPTS += --disable-libxml2
endif

# ffmpeg freetype support require fenv.h which is only
# available/working on glibc.
# The microblaze variant doesn't provide the needed exceptions
ifeq ($(BR2_PACKAGE_FREETYPE)$(BR2_TOOLCHAIN_USES_GLIBC)x$(BR2_microblaze),yyx)
NX_FFMPEG_CONF_OPTS += --enable-libfreetype
NX_FFMPEG_DEPENDENCIES += freetype
else
NX_FFMPEG_CONF_OPTS += --disable-libfreetype
endif

ifeq ($(BR2_PACKAGE_FONTCONFIG),y)
NX_FFMPEG_CONF_OPTS += --enable-fontconfig
NX_FFMPEG_DEPENDENCIES += fontconfig
else
NX_FFMPEG_CONF_OPTS += --disable-fontconfig
endif

ifeq ($(BR2_PACKAGE_LIBFRIBIDI),y)
NX_FFMPEG_CONF_OPTS += --enable-libfribidi
NX_FFMPEG_DEPENDENCIES += libfribidi
else
NX_FFMPEG_CONF_OPTS += --disable-libfribidi
endif

ifeq ($(BR2_PACKAGE_OPENJPEG),y)
NX_FFMPEG_CONF_OPTS += --enable-libopenjpeg
NX_FFMPEG_DEPENDENCIES += openjpeg
else
NX_FFMPEG_CONF_OPTS += --disable-libopenjpeg
endif

ifeq ($(BR2_PACKAGE_X264)$(BR2_PACKAGE_NX_FFMPEG_GPL),yy)
NX_FFMPEG_CONF_OPTS += --enable-libx264
NX_FFMPEG_DEPENDENCIES += x264
else
NX_FFMPEG_CONF_OPTS += --disable-libx264
endif

ifeq ($(BR2_PACKAGE_X265)$(BR2_PACKAGE_NX_FFMPEG_GPL),yy)
NX_FFMPEG_CONF_OPTS += --enable-libx265
NX_FFMPEG_DEPENDENCIES += x265
else
NX_FFMPEG_CONF_OPTS += --disable-libx265
endif

ifeq ($(BR2_PACKAGE_DAV1D),y)
NX_FFMPEG_CONF_OPTS += --enable-libdav1d
NX_FFMPEG_DEPENDENCIES += dav1d
else
NX_FFMPEG_CONF_OPTS += --disable-libdav1d
endif

ifeq ($(BR2_X86_CPU_HAS_MMX),y)
NX_FFMPEG_CONF_OPTS += --enable-x86asm
NX_FFMPEG_DEPENDENCIES += host-nasm
else
NX_FFMPEG_CONF_OPTS += --disable-x86asm
NX_FFMPEG_CONF_OPTS += --disable-mmx
endif

ifeq ($(BR2_X86_CPU_HAS_SSE),y)
NX_FFMPEG_CONF_OPTS += --enable-sse
else
NX_FFMPEG_CONF_OPTS += --disable-sse
endif

ifeq ($(BR2_X86_CPU_HAS_SSE2),y)
NX_FFMPEG_CONF_OPTS += --enable-sse2
else
NX_FFMPEG_CONF_OPTS += --disable-sse2
endif

ifeq ($(BR2_X86_CPU_HAS_SSE3),y)
NX_FFMPEG_CONF_OPTS += --enable-sse3
else
NX_FFMPEG_CONF_OPTS += --disable-sse3
endif

ifeq ($(BR2_X86_CPU_HAS_SSSE3),y)
NX_FFMPEG_CONF_OPTS += --enable-ssse3
else
NX_FFMPEG_CONF_OPTS += --disable-ssse3
endif

ifeq ($(BR2_X86_CPU_HAS_SSE4),y)
NX_FFMPEG_CONF_OPTS += --enable-sse4
else
NX_FFMPEG_CONF_OPTS += --disable-sse4
endif

ifeq ($(BR2_X86_CPU_HAS_SSE42),y)
NX_FFMPEG_CONF_OPTS += --enable-sse42
else
NX_FFMPEG_CONF_OPTS += --disable-sse42
endif

ifeq ($(BR2_X86_CPU_HAS_AVX),y)
NX_FFMPEG_CONF_OPTS += --enable-avx
else
NX_FFMPEG_CONF_OPTS += --disable-avx
endif

ifeq ($(BR2_X86_CPU_HAS_AVX2),y)
NX_FFMPEG_CONF_OPTS += --enable-avx2
else
NX_FFMPEG_CONF_OPTS += --disable-avx2
endif

# Explicitly disable everything that doesn't match for ARM
# FFMPEG "autodetects" by compiling an extended instruction via AS
# This works on compilers that aren't built for generic by default
ifeq ($(BR2_ARM_CPU_ARMV4),y)
NX_FFMPEG_CONF_OPTS += --disable-armv5te
endif
ifeq ($(BR2_ARM_CPU_ARMV6)$(BR2_ARM_CPU_ARMV7A),y)
NX_FFMPEG_CONF_OPTS += --enable-armv6
else
NX_FFMPEG_CONF_OPTS += --disable-armv6 --disable-armv6t2
endif
ifeq ($(BR2_ARM_CPU_HAS_VFPV2),y)
NX_FFMPEG_CONF_OPTS += --enable-vfp
else
NX_FFMPEG_CONF_OPTS += --disable-vfp
endif
ifeq ($(BR2_ARM_CPU_HAS_NEON),y)
NX_FFMPEG_CONF_OPTS += --enable-neon
else ifeq ($(BR2_aarch64),y)
NX_FFMPEG_CONF_OPTS += --enable-neon
else
NX_FFMPEG_CONF_OPTS += --disable-neon
endif

ifeq ($(BR2_mips)$(BR2_mipsel)$(BR2_mips64)$(BR2_mips64el),y)
ifeq ($(BR2_MIPS_SOFT_FLOAT),y)
NX_FFMPEG_CONF_OPTS += --disable-mipsfpu
else
NX_FFMPEG_CONF_OPTS += --enable-mipsfpu
endif

# Fix build failure on several missing assembly instructions
NX_FFMPEG_CONF_OPTS += --disable-asm
endif # MIPS

ifeq ($(BR2_POWERPC_CPU_HAS_ALTIVEC):$(BR2_powerpc64le),y:)
NX_FFMPEG_CONF_OPTS += --enable-altivec
else ifeq ($(BR2_POWERPC_CPU_HAS_VSX):$(BR2_powerpc64le),y:y)
# On LE, ffmpeg AltiVec support needs VSX intrinsics, and VSX
# is an extension to AltiVec.
NX_FFMPEG_CONF_OPTS += --enable-altivec
else
NX_FFMPEG_CONF_OPTS += --disable-altivec
endif

# Fix build failure on several missing assembly instructions
ifeq ($(BR2_RISCV_32),y)
NX_FFMPEG_CONF_OPTS += --disable-rvv --disable-asm
endif

# Uses __atomic_fetch_add_4
ifeq ($(BR2_TOOLCHAIN_HAS_LIBATOMIC),y)
NX_FFMPEG_CONF_OPTS += --extra-libs=-latomic
endif

ifeq ($(BR2_STATIC_LIBS),)
NX_FFMPEG_CONF_OPTS += --enable-pic
else
NX_FFMPEG_CONF_OPTS += --disable-pic
endif

# Default to --cpu=generic for MIPS architecture, in order to avoid a
# warning from ffmpeg's configure script.
ifeq ($(BR2_mips)$(BR2_mipsel)$(BR2_mips64)$(BR2_mips64el),y)
NX_FFMPEG_CONF_OPTS += --cpu=generic
else ifneq ($(GCC_TARGET_CPU),)
NX_FFMPEG_CONF_OPTS += --cpu="$(GCC_TARGET_CPU)"
else ifneq ($(GCC_TARGET_ARCH),)
NX_FFMPEG_CONF_OPTS += --cpu="$(GCC_TARGET_ARCH)"
endif

NX_FFMPEG_CFLAGS = $(TARGET_CFLAGS)

ifeq ($(BR2_TOOLCHAIN_HAS_GCC_BUG_85180),y)
NX_FFMPEG_CONF_OPTS += --disable-optimizations
NX_FFMPEG_CFLAGS += -O0
endif

ifeq ($(BR2_TOOLCHAIN_HAS_GCC_BUG_68485),y)
NX_FFMPEG_CONF_OPTS += --disable-optimizations
NX_FFMPEG_CFLAGS += -O0
endif

ifeq ($(BR2_ARM_INSTRUCTIONS_THUMB),y)
NX_FFMPEG_CFLAGS += -marm
endif

NX_FFMPEG_CONF_ENV += CFLAGS="$(NX_FFMPEG_CFLAGS)"
NX_FFMPEG_CONF_OPTS += $(call qstrip,$(BR2_PACKAGE_NX_FFMPEG_EXTRACONF))

# Override NX_FFMPEG_CONFIGURE_CMDS: FFmpeg does not support --target and others
define NX_FFMPEG_CONFIGURE_CMDS
	(cd $(NX_FFMPEG_SRCDIR) && rm -rf config.cache && \
	$(TARGET_CONFIGURE_OPTS) \
	$(TARGET_CONFIGURE_ARGS) \
	$(NX_FFMPEG_CONF_ENV) \
	./configure \
		--enable-cross-compile \
		--cross-prefix=$(TARGET_CROSS) \
		--sysroot=$(STAGING_DIR) \
		--host-cc="$(HOSTCC)" \
		--arch=$(BR2_ARCH) \
		--target-os="linux" \
		--disable-stripping \
		--pkg-config="$(PKG_CONFIG_HOST_BINARY)" \
		$(SHARED_STATIC_LIBS_OPTS) \
		$(NX_FFMPEG_CONF_OPTS) \
	)
endef

define NX_FFMPEG_REMOVE_EXAMPLE_SRC_FILES
	rm -rf $(TARGET_DIR)/usr/share/ffmpeg/examples
endef
NX_FFMPEG_POST_INSTALL_TARGET_HOOKS += NX_FFMPEG_REMOVE_EXAMPLE_SRC_FILES

$(eval $(autotools-package))
