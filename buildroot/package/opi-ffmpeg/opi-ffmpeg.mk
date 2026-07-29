################################################################################
#
# opi-ffmpeg
#
################################################################################

OPI_FFMPEG_VERSION = v4l2-request-n7.1.3
OPI_FFMPEG_SITE = https://code.ffmpeg.org/Kwiboo/FFmpeg.git
OPI_FFMPEG_SITE_METHOD = git
OPI_FFMPEG_INSTALL_STAGING = YES

OPI_FFMPEG_LICENSE = LGPL-2.1+, libjpeg license
OPI_FFMPEG_LICENSE_FILES = LICENSE.md COPYING.LGPLv2.1
ifeq ($(BR2_PACKAGE_OPI_FFMPEG_GPL),y)
OPI_FFMPEG_LICENSE += and GPL-2.0+
OPI_FFMPEG_LICENSE_FILES += COPYING.GPLv2
endif

OPI_FFMPEG_CONF_OPTS = \
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

# Orange Pi will always need this for Cedrus
ifeq ($(BR2_PACKAGE_EUDEV),y)
OPI_FFMPEG_CONF_OPTS += --enable-libudev
OPI_FFMPEG_DEPENDENCIES += eudev
else
OPI_FFMPEG_CONF_OPTS += --disable-libudev
endif

OPI_FFMPEG_DEPENDENCIES += host-pkgconf

ifeq ($(BR2_PACKAGE_OPI_FFMPEG_GPL),y)
OPI_FFMPEG_CONF_OPTS += --enable-gpl
else
OPI_FFMPEG_CONF_OPTS += --disable-gpl
endif

ifeq ($(BR2_PACKAGE_OPI_FFMPEG_NONFREE),y)
OPI_FFMPEG_CONF_OPTS += --enable-nonfree
else
OPI_FFMPEG_CONF_OPTS += --disable-nonfree
endif

ifeq ($(BR2_PACKAGE_OPI_FFMPEG_FFMPEG),y)
OPI_FFMPEG_CONF_OPTS += --enable-ffmpeg
else
OPI_FFMPEG_CONF_OPTS += --disable-ffmpeg
endif

ifeq ($(BR2_PACKAGE_OPI_FFMPEG_FFPLAY),y)
OPI_FFMPEG_DEPENDENCIES += sdl2
OPI_FFMPEG_CONF_OPTS += --enable-ffplay
OPI_FFMPEG_CONF_ENV += SDL_CONFIG=$(STAGING_DIR)/usr/bin/sdl2-config
else
OPI_FFMPEG_CONF_OPTS += --disable-ffplay
endif

ifeq ($(BR2_PACKAGE_JACK1),y)
OPI_FFMPEG_CONF_OPTS += --enable-libjack
OPI_FFMPEG_DEPENDENCIES += jack1
else ifeq ($(BR2_PACKAGE_JACK2),y)
OPI_FFMPEG_CONF_OPTS += --enable-libjack
OPI_FFMPEG_DEPENDENCIES += jack2
else
OPI_FFMPEG_CONF_OPTS += --disable-libjack
endif

ifeq ($(BR2_PACKAGE_LIBV4L),y)
OPI_FFMPEG_DEPENDENCIES += libv4l
OPI_FFMPEG_CONF_OPTS += --enable-libv4l2
else
OPI_FFMPEG_CONF_OPTS += --disable-libv4l2
endif

ifeq ($(BR2_PACKAGE_OPI_FFMPEG_FFPROBE),y)
OPI_FFMPEG_CONF_OPTS += --enable-ffprobe
else
OPI_FFMPEG_CONF_OPTS += --disable-ffprobe
endif

ifeq ($(BR2_PACKAGE_OPI_FFMPEG_XCBGRAB),y)
OPI_FFMPEG_CONF_OPTS += \
	--enable-libxcb \
	--enable-libxcb-shape \
	--enable-libxcb-shm \
	--enable-libxcb-xfixes
OPI_FFMPEG_DEPENDENCIES += libxcb
else
OPI_FFMPEG_CONF_OPTS += --disable-libxcb
endif

ifeq ($(BR2_PACKAGE_OPI_FFMPEG_POSTPROC),y)
OPI_FFMPEG_CONF_OPTS += --enable-postproc
else
OPI_FFMPEG_CONF_OPTS += --disable-postproc
endif

ifeq ($(BR2_PACKAGE_OPI_FFMPEG_SWSCALE),y)
OPI_FFMPEG_CONF_OPTS += --enable-swscale
else
OPI_FFMPEG_CONF_OPTS += --disable-swscale
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_ENCODERS)),all)
OPI_FFMPEG_CONF_OPTS += --disable-encoders \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_ENCODERS)),--enable-encoder=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_DECODERS)),all)
OPI_FFMPEG_CONF_OPTS += --disable-decoders \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_DECODERS)),--enable-decoder=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_MUXERS)),all)
OPI_FFMPEG_CONF_OPTS += --disable-muxers \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_MUXERS)),--enable-muxer=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_DEMUXERS)),all)
OPI_FFMPEG_CONF_OPTS += --disable-demuxers \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_DEMUXERS)),--enable-demuxer=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_PARSERS)),all)
OPI_FFMPEG_CONF_OPTS += --disable-parsers \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_PARSERS)),--enable-parser=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_BSFS)),all)
OPI_FFMPEG_CONF_OPTS += --disable-bsfs \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_BSFS)),--enable-bsf=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_PROTOCOLS)),all)
OPI_FFMPEG_CONF_OPTS += --disable-protocols \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_PROTOCOLS)),--enable-protocol=$(x))
endif

ifneq ($(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_FILTERS)),all)
OPI_FFMPEG_CONF_OPTS += --disable-filters \
	$(foreach x,$(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_FILTERS)),--enable-filter=$(x))
endif

ifeq ($(BR2_PACKAGE_OPI_FFMPEG_INDEVS),y)
OPI_FFMPEG_CONF_OPTS += --enable-indevs
ifeq ($(BR2_PACKAGE_ALSA_LIB),y)
OPI_FFMPEG_CONF_OPTS += --enable-alsa
OPI_FFMPEG_DEPENDENCIES += alsa-lib
else
OPI_FFMPEG_CONF_OPTS += --disable-alsa
endif
else
OPI_FFMPEG_CONF_OPTS += --disable-indevs
endif

ifeq ($(BR2_PACKAGE_OPI_FFMPEG_OUTDEVS),y)
OPI_FFMPEG_CONF_OPTS += --enable-outdevs
ifeq ($(BR2_PACKAGE_ALSA_LIB),y)
OPI_FFMPEG_DEPENDENCIES += alsa-lib
endif
else
OPI_FFMPEG_CONF_OPTS += --disable-outdevs
endif

ifeq ($(BR2_TOOLCHAIN_HAS_THREADS),y)
OPI_FFMPEG_CONF_OPTS += --enable-pthreads
else
OPI_FFMPEG_CONF_OPTS += --disable-pthreads
endif

ifeq ($(BR2_PACKAGE_ZLIB),y)
OPI_FFMPEG_CONF_OPTS += --enable-zlib
OPI_FFMPEG_DEPENDENCIES += zlib
else
OPI_FFMPEG_CONF_OPTS += --disable-zlib
endif

ifeq ($(BR2_PACKAGE_BZIP2),y)
OPI_FFMPEG_CONF_OPTS += --enable-bzlib
OPI_FFMPEG_DEPENDENCIES += bzip2
else
OPI_FFMPEG_CONF_OPTS += --disable-bzlib
endif

ifeq ($(BR2_PACKAGE_FDK_AAC)$(BR2_PACKAGE_OPI_FFMPEG_NONFREE),yy)
OPI_FFMPEG_CONF_OPTS += --enable-libfdk-aac
OPI_FFMPEG_DEPENDENCIES += fdk-aac
else
OPI_FFMPEG_CONF_OPTS += --disable-libfdk-aac
endif

ifeq ($(BR2_PACKAGE_OPI_FFMPEG_GPL)$(BR2_PACKAGE_LIBCDIO_PARANOIA),yy)
OPI_FFMPEG_CONF_OPTS += --enable-libcdio
OPI_FFMPEG_DEPENDENCIES += libcdio-paranoia
else
OPI_FFMPEG_CONF_OPTS += --disable-libcdio
endif

ifeq ($(BR2_PACKAGE_GNUTLS),y)
OPI_FFMPEG_CONF_OPTS += --enable-gnutls --disable-openssl
OPI_FFMPEG_DEPENDENCIES += gnutls
else
OPI_FFMPEG_CONF_OPTS += --disable-gnutls
ifeq ($(BR2_PACKAGE_OPENSSL),y)
# openssl isn't license compatible with GPL
ifeq ($(BR2_PACKAGE_OPI_FFMPEG_GPL)x$(BR2_PACKAGE_OPI_FFMPEG_NONFREE),yx)
OPI_FFMPEG_CONF_OPTS += --disable-openssl
else
OPI_FFMPEG_CONF_OPTS += --enable-openssl
OPI_FFMPEG_DEPENDENCIES += openssl
endif
else
OPI_FFMPEG_CONF_OPTS += --disable-openssl
endif
endif

ifeq ($(BR2_PACKAGE_LIBDRM),y)
OPI_FFMPEG_CONF_OPTS += --enable-libdrm
OPI_FFMPEG_DEPENDENCIES += libdrm
else
OPI_FFMPEG_CONF_OPTS += --disable-libdrm
endif

ifeq ($(BR2_PACKAGE_LIBOPENH264),y)
OPI_FFMPEG_CONF_OPTS += --enable-libopenh264
OPI_FFMPEG_DEPENDENCIES += libopenh264
else
OPI_FFMPEG_CONF_OPTS += --disable-libopenh264
endif

ifeq ($(BR2_PACKAGE_LIBVORBIS),y)
OPI_FFMPEG_DEPENDENCIES += libvorbis
OPI_FFMPEG_CONF_OPTS += \
	--enable-libvorbis \
	--enable-muxer=ogg \
	--enable-encoder=libvorbis
endif

ifeq ($(BR2_PACKAGE_LIBVA),y)
OPI_FFMPEG_CONF_OPTS += --enable-vaapi
OPI_FFMPEG_DEPENDENCIES += libva
else
OPI_FFMPEG_CONF_OPTS += --disable-vaapi
endif

ifeq ($(BR2_PACKAGE_LIBVDPAU),y)
OPI_FFMPEG_CONF_OPTS += --enable-vdpau
OPI_FFMPEG_DEPENDENCIES += libvdpau
else
OPI_FFMPEG_CONF_OPTS += --disable-vdpau
endif

ifeq ($(BR2_PACKAGE_RPI_USERLAND),y)
OPI_FFMPEG_CONF_OPTS += --enable-omx --enable-omx-rpi \
	--extra-cflags=-I$(STAGING_DIR)/usr/include/IL
OPI_FFMPEG_DEPENDENCIES += rpi-userland
ifeq ($(BR2_arm),y)
OPI_FFMPEG_CONF_OPTS += --enable-mmal
else
OPI_FFMPEG_CONF_OPTS += --disable-mmal
endif
else
OPI_FFMPEG_CONF_OPTS += --disable-mmal --disable-omx --disable-omx-rpi
endif

# To avoid a circular dependency only use opencv if opencv itself does
# not depend on ffmpeg.
ifeq ($(BR2_PACKAGE_OPENCV3_LIB_IMGPROC)x$(BR2_PACKAGE_OPENCV3_WITH_FFMPEG),yx)
OPI_FFMPEG_CONF_OPTS += --enable-libopencv
OPI_FFMPEG_DEPENDENCIES += opencv3
else
OPI_FFMPEG_CONF_OPTS += --disable-libopencv
endif

ifeq ($(BR2_PACKAGE_OPUS),y)
OPI_FFMPEG_CONF_OPTS += --enable-libopus
OPI_FFMPEG_DEPENDENCIES += opus
else
OPI_FFMPEG_CONF_OPTS += --disable-libopus
endif

ifeq ($(BR2_PACKAGE_LIBVPX),y)
OPI_FFMPEG_CONF_OPTS += --enable-libvpx
OPI_FFMPEG_DEPENDENCIES += libvpx
else
OPI_FFMPEG_CONF_OPTS += --disable-libvpx
endif

ifeq ($(BR2_PACKAGE_LIBASS),y)
OPI_FFMPEG_CONF_OPTS += --enable-libass
OPI_FFMPEG_DEPENDENCIES += libass
else
OPI_FFMPEG_CONF_OPTS += --disable-libass
endif

ifeq ($(BR2_PACKAGE_LIBBLURAY),y)
OPI_FFMPEG_CONF_OPTS += --enable-libbluray
OPI_FFMPEG_DEPENDENCIES += libbluray
else
OPI_FFMPEG_CONF_OPTS += --disable-libbluray
endif

ifeq ($(BR2_PACKAGE_RTMPDUMP),y)
OPI_FFMPEG_CONF_OPTS += --enable-librtmp
OPI_FFMPEG_DEPENDENCIES += rtmpdump
else
OPI_FFMPEG_CONF_OPTS += --disable-librtmp
endif

ifeq ($(BR2_PACKAGE_LAME),y)
OPI_FFMPEG_CONF_OPTS += --enable-libmp3lame
OPI_FFMPEG_DEPENDENCIES += lame
else
OPI_FFMPEG_CONF_OPTS += --disable-libmp3lame
endif

ifeq ($(BR2_PACKAGE_LIBMODPLUG),y)
OPI_FFMPEG_CONF_OPTS += --enable-libmodplug
OPI_FFMPEG_DEPENDENCIES += libmodplug
else
OPI_FFMPEG_CONF_OPTS += --disable-libmodplug
endif

ifeq ($(BR2_PACKAGE_LIBOPENMPT),y)
OPI_FFMPEG_CONF_OPTS += --enable-libopenmpt
OPI_FFMPEG_DEPENDENCIES += libopenmpt
else
OPI_FFMPEG_CONF_OPTS += --disable-libopenmpt
endif

ifeq ($(BR2_PACKAGE_SPEEX),y)
OPI_FFMPEG_CONF_OPTS += --enable-libspeex
OPI_FFMPEG_DEPENDENCIES += speex
else
OPI_FFMPEG_CONF_OPTS += --disable-libspeex
endif

ifeq ($(BR2_PACKAGE_LIBTHEORA),y)
OPI_FFMPEG_CONF_OPTS += --enable-libtheora
OPI_FFMPEG_DEPENDENCIES += libtheora
else
OPI_FFMPEG_CONF_OPTS += --disable-libtheora
endif

ifeq ($(BR2_PACKAGE_LIBICONV),y)
OPI_FFMPEG_CONF_OPTS += --enable-iconv
OPI_FFMPEG_DEPENDENCIES += libiconv
else
OPI_FFMPEG_CONF_OPTS += --disable-iconv
endif

ifeq ($(BR2_PACKAGE_LIBXML2),y)
OPI_FFMPEG_CONF_OPTS += --enable-libxml2
OPI_FFMPEG_DEPENDENCIES += libxml2
else
OPI_FFMPEG_CONF_OPTS += --disable-libxml2
endif

# ffmpeg freetype support require fenv.h which is only
# available/working on glibc.
# The microblaze variant doesn't provide the needed exceptions
ifeq ($(BR2_PACKAGE_FREETYPE)$(BR2_TOOLCHAIN_USES_GLIBC)x$(BR2_microblaze),yyx)
OPI_FFMPEG_CONF_OPTS += --enable-libfreetype
OPI_FFMPEG_DEPENDENCIES += freetype
else
OPI_FFMPEG_CONF_OPTS += --disable-libfreetype
endif

ifeq ($(BR2_PACKAGE_FONTCONFIG),y)
OPI_FFMPEG_CONF_OPTS += --enable-fontconfig
OPI_FFMPEG_DEPENDENCIES += fontconfig
else
OPI_FFMPEG_CONF_OPTS += --disable-fontconfig
endif

ifeq ($(BR2_PACKAGE_LIBFRIBIDI),y)
OPI_FFMPEG_CONF_OPTS += --enable-libfribidi
OPI_FFMPEG_DEPENDENCIES += libfribidi
else
OPI_FFMPEG_CONF_OPTS += --disable-libfribidi
endif

ifeq ($(BR2_PACKAGE_OPENJPEG),y)
OPI_FFMPEG_CONF_OPTS += --enable-libopenjpeg
OPI_FFMPEG_DEPENDENCIES += openjpeg
else
OPI_FFMPEG_CONF_OPTS += --disable-libopenjpeg
endif

ifeq ($(BR2_PACKAGE_X264)$(BR2_PACKAGE_OPI_FFMPEG_GPL),yy)
OPI_FFMPEG_CONF_OPTS += --enable-libx264
OPI_FFMPEG_DEPENDENCIES += x264
else
OPI_FFMPEG_CONF_OPTS += --disable-libx264
endif

ifeq ($(BR2_PACKAGE_X265)$(BR2_PACKAGE_OPI_FFMPEG_GPL),yy)
OPI_FFMPEG_CONF_OPTS += --enable-libx265
OPI_FFMPEG_DEPENDENCIES += x265
else
OPI_FFMPEG_CONF_OPTS += --disable-libx265
endif

ifeq ($(BR2_PACKAGE_DAV1D),y)
OPI_FFMPEG_CONF_OPTS += --enable-libdav1d
OPI_FFMPEG_DEPENDENCIES += dav1d
else
OPI_FFMPEG_CONF_OPTS += --disable-libdav1d
endif

ifeq ($(BR2_X86_CPU_HAS_MMX),y)
OPI_FFMPEG_CONF_OPTS += --enable-x86asm
OPI_FFMPEG_DEPENDENCIES += host-nasm
else
OPI_FFMPEG_CONF_OPTS += --disable-x86asm
OPI_FFMPEG_CONF_OPTS += --disable-mmx
endif

ifeq ($(BR2_X86_CPU_HAS_SSE),y)
OPI_FFMPEG_CONF_OPTS += --enable-sse
else
OPI_FFMPEG_CONF_OPTS += --disable-sse
endif

ifeq ($(BR2_X86_CPU_HAS_SSE2),y)
OPI_FFMPEG_CONF_OPTS += --enable-sse2
else
OPI_FFMPEG_CONF_OPTS += --disable-sse2
endif

ifeq ($(BR2_X86_CPU_HAS_SSE3),y)
OPI_FFMPEG_CONF_OPTS += --enable-sse3
else
OPI_FFMPEG_CONF_OPTS += --disable-sse3
endif

ifeq ($(BR2_X86_CPU_HAS_SSSE3),y)
OPI_FFMPEG_CONF_OPTS += --enable-ssse3
else
OPI_FFMPEG_CONF_OPTS += --disable-ssse3
endif

ifeq ($(BR2_X86_CPU_HAS_SSE4),y)
OPI_FFMPEG_CONF_OPTS += --enable-sse4
else
OPI_FFMPEG_CONF_OPTS += --disable-sse4
endif

ifeq ($(BR2_X86_CPU_HAS_SSE42),y)
OPI_FFMPEG_CONF_OPTS += --enable-sse42
else
OPI_FFMPEG_CONF_OPTS += --disable-sse42
endif

ifeq ($(BR2_X86_CPU_HAS_AVX),y)
OPI_FFMPEG_CONF_OPTS += --enable-avx
else
OPI_FFMPEG_CONF_OPTS += --disable-avx
endif

ifeq ($(BR2_X86_CPU_HAS_AVX2),y)
OPI_FFMPEG_CONF_OPTS += --enable-avx2
else
OPI_FFMPEG_CONF_OPTS += --disable-avx2
endif

# Explicitly disable everything that doesn't match for ARM
# FFMPEG "autodetects" by compiling an extended instruction via AS
# This works on compilers that aren't built for generic by default
ifeq ($(BR2_ARM_CPU_ARMV4),y)
OPI_FFMPEG_CONF_OPTS += --disable-armv5te
endif
ifeq ($(BR2_ARM_CPU_ARMV6)$(BR2_ARM_CPU_ARMV7A),y)
OPI_FFMPEG_CONF_OPTS += --enable-armv6
else
OPI_FFMPEG_CONF_OPTS += --disable-armv6 --disable-armv6t2
endif
ifeq ($(BR2_ARM_CPU_HAS_VFPV2),y)
OPI_FFMPEG_CONF_OPTS += --enable-vfp
else
OPI_FFMPEG_CONF_OPTS += --disable-vfp
endif
ifeq ($(BR2_ARM_CPU_HAS_NEON),y)
OPI_FFMPEG_CONF_OPTS += --enable-neon
else ifeq ($(BR2_aarch64),y)
OPI_FFMPEG_CONF_OPTS += --enable-neon
else
OPI_FFMPEG_CONF_OPTS += --disable-neon
endif

ifeq ($(BR2_mips)$(BR2_mipsel)$(BR2_mips64)$(BR2_mips64el),y)
ifeq ($(BR2_MIPS_SOFT_FLOAT),y)
OPI_FFMPEG_CONF_OPTS += --disable-mipsfpu
else
OPI_FFMPEG_CONF_OPTS += --enable-mipsfpu
endif

# Fix build failure on several missing assembly instructions
OPI_FFMPEG_CONF_OPTS += --disable-asm
endif # MIPS

ifeq ($(BR2_POWERPC_CPU_HAS_ALTIVEC):$(BR2_powerpc64le),y:)
OPI_FFMPEG_CONF_OPTS += --enable-altivec
else ifeq ($(BR2_POWERPC_CPU_HAS_VSX):$(BR2_powerpc64le),y:y)
# On LE, ffmpeg AltiVec support needs VSX intrinsics, and VSX
# is an extension to AltiVec.
OPI_FFMPEG_CONF_OPTS += --enable-altivec
else
OPI_FFMPEG_CONF_OPTS += --disable-altivec
endif

# Fix build failure on several missing assembly instructions
ifeq ($(BR2_RISCV_32),y)
OPI_FFMPEG_CONF_OPTS += --disable-rvv --disable-asm
endif

# Uses __atomic_fetch_add_4
ifeq ($(BR2_TOOLCHAIN_HAS_LIBATOMIC),y)
OPI_FFMPEG_CONF_OPTS += --extra-libs=-latomic
endif

ifeq ($(BR2_STATIC_LIBS),)
OPI_FFMPEG_CONF_OPTS += --enable-pic
else
OPI_FFMPEG_CONF_OPTS += --disable-pic
endif

# Default to --cpu=generic for MIPS architecture, in order to avoid a
# warning from ffmpeg's configure script.
ifeq ($(BR2_mips)$(BR2_mipsel)$(BR2_mips64)$(BR2_mips64el),y)
OPI_FFMPEG_CONF_OPTS += --cpu=generic
else ifneq ($(GCC_TARGET_CPU),)
OPI_FFMPEG_CONF_OPTS += --cpu="$(GCC_TARGET_CPU)"
else ifneq ($(GCC_TARGET_ARCH),)
OPI_FFMPEG_CONF_OPTS += --cpu="$(GCC_TARGET_ARCH)"
endif

OPI_FFMPEG_CFLAGS = $(TARGET_CFLAGS)

ifeq ($(BR2_TOOLCHAIN_HAS_GCC_BUG_85180),y)
OPI_FFMPEG_CONF_OPTS += --disable-optimizations
OPI_FFMPEG_CFLAGS += -O0
endif

ifeq ($(BR2_TOOLCHAIN_HAS_GCC_BUG_68485),y)
OPI_FFMPEG_CONF_OPTS += --disable-optimizations
OPI_FFMPEG_CFLAGS += -O0
endif

ifeq ($(BR2_ARM_INSTRUCTIONS_THUMB),y)
OPI_FFMPEG_CFLAGS += -marm
endif

OPI_FFMPEG_CONF_ENV += CFLAGS="$(OPI_FFMPEG_CFLAGS)"
OPI_FFMPEG_CONF_OPTS += $(call qstrip,$(BR2_PACKAGE_OPI_FFMPEG_EXTRACONF))

# Override OPI_FFMPEG_CONFIGURE_CMDS: FFmpeg does not support --target and others
define OPI_FFMPEG_CONFIGURE_CMDS
	(cd $(OPI_FFMPEG_SRCDIR) && rm -rf config.cache && \
	$(TARGET_CONFIGURE_OPTS) \
	$(TARGET_CONFIGURE_ARGS) \
	$(OPI_FFMPEG_CONF_ENV) \
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
		$(OPI_FFMPEG_CONF_OPTS) \
	)
endef

define OPI_FFMPEG_REMOVE_EXAMPLE_SRC_FILES
	rm -rf $(TARGET_DIR)/usr/share/ffmpeg/examples
endef
OPI_FFMPEG_POST_INSTALL_TARGET_HOOKS += OPI_FFMPEG_REMOVE_EXAMPLE_SRC_FILES

$(eval $(autotools-package))
