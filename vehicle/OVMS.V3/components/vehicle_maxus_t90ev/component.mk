# Main component makefile for the Maxus T90 EV (OVMS v3)
#
# This Makefile ensures that the source files in the 'src' directory
# are compiled and linked into the component library only when
# the Maxus T90 EV vehicle type is selected in the Kconfig menu.

ifdef CONFIG_OVMS_VEHICLE_MAXT90EV
COMPONENT_ADD_INCLUDEDIRS:=src
COMPONENT_SRCDIRS:=src
# Standard linker flags to force linking of the static library (lib<component_name>.a)
COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
endif

# If you need to build the component regardless of the vehicle selected, 
# you can comment out the 'ifdef' and 'endif' lines.
