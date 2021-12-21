#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

COMPONENT_SRCDIRS := src
COMPONENT_ADD_INCLUDEDIRS := src
COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
COMPONENT_EMBED_FILES := trustedca/usertrust.crt trustedca/digicert_global.crt trustedca/starfield_class2.crt trustedca/baltimore_cybertrust.crt trustedca/isrg_x1.crt
