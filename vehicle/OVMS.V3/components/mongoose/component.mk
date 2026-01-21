#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
COMPONENT_EXTRA_CLEAN := $(COMPONENT_PATH)/include/mg_version.h

mongoose/mongoose.o: $(COMPONENT_PATH)/include/mg_version.h

$(COMPONENT_PATH)/include/mg_version.h: SHELL := /bin/bash
$(COMPONENT_PATH)/include/mg_version.h: $(COMPONENT_PATH)/mongoose/mongoose.h
	{ \
		MG_VERSION=$$(cat "$(COMPONENT_PATH)/mongoose/mongoose.h" | \
		grep "#define MG_VERSION" | \
		cut "-d" " " "-f3" | \
		sed -e s/\"//g); \
		echo "Compiled Mongoose version: $${MG_VERSION}"; \
		a=( $${MG_VERSION//./ } ); \
		cat "$(COMPONENT_PATH)/mg_version.h.in" | \
			sed -e "s/@MG_VERSION_MAJOR@/$${a[0]}/" \
					-e "s/@MG_VERSION_MINOR@/$${a[1]}/" \
					-e "s/@MG_VERSION_PATCH@/0/" \
			> "$(COMPONENT_PATH)/include/mg_version.h"; \
	}
COMPONENT_ADD_INCLUDEDIRS := include mongoose src
CFLAGS += -DMG_ENABLE_LINES  # Only for Mongoose >= 7.0
CFLAGS += -DMG_ENABLE_LWIP   # Only for Mongoose >= 7.0
ifdef CONFIG_MG_SSL_IF_WOLFSSL
COMPONENT_PRIV_INCLUDEDIRS := ../wolfssl ../wolfssl/wolfssl
COMPONENT_EXTRA_INCLUDES := ${IDF_PATH}/components/freertos/include/freertos
endif
ifdef CONFIG_MG_SSL_IF_MBEDTLS
CFLAGS += -DMG_ENABLE_MBEDTLS=1  # Only for Mongoose >= 7.0
endif
COMPONENT_SRCDIRS := mongoose src
COMPONENT_SUBMODULES := mongoose
#COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
endif
