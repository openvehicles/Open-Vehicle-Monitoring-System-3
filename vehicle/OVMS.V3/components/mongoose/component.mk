#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
COMPONENT_ADD_INCLUDEDIRS:=.
COMPONENT_SRCDIRS:=.
#COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
#else
COMPONENT_ADD_INCLUDEDIRS:=stub
COMPONENT_SRCDIRS:=stub
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
