#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#


ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
COMPONENT_DEPENDS := mongoose
COMPONENT_SRCDIRS := src srcduk
COMPONENT_ADD_INCLUDEDIRS := src srcduk umm
# To get line numbers of internal modules in stack traces, embed the uncompressed sources:
#COMPONENT_EMBED_FILES := jsmodsrc/pubsub.js jsmodsrc/json.js
COMPONENT_EMBED_FILES := jsmodembed/pubsub.js jsmodembed/json.js
else
COMPONENT_SRCDIRS := src
COMPONENT_ADD_INCLUDEDIRS := src
endif

COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
