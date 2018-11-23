#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_COMP_RE_TOOLS

COMPONENT_ADD_INCLUDEDIRS:=src yacclex
COMPONENT_SRCDIRS:=src yacclex
COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
COMPONENT_OBJS = src/dbc_app.o src/dbc.o yacclex/dbc_tokeniser.o yacclex/dbc_parser.o

COMPONENT_EXTRA_CLEAN := yacclex/dbc_tokeniser.c yacclex/dbc_parser.hpp yacclex/dbc_parser.cpp

$(COMPONENT_PATH)/yacclex/dbc_tokeniser.c : $(COMPONENT_PATH)/src/dbc_tokeniser.l $(COMPONENT_PATH)/yacclex/dbc_parser.hpp
	lex -o $(COMPONENT_PATH)/yacclex/dbc_tokeniser.c $(COMPONENT_PATH)/src/dbc_tokeniser.l

$(COMPONENT_PATH)/yacclex/dbc_parser.hpp : $(COMPONENT_PATH)/src/dbc_parser.y
	yacc -o $(COMPONENT_PATH)/yacclex/dbc_parser.cpp -d $(COMPONENT_PATH)/src/dbc_parser.y

dbc.o: \
	$(COMPONENT_PATH)/yacclex/dbc_tokeniser.c \
	$(COMPONENT_PATH)/yacclex/dbc_parser.hpp

endif
