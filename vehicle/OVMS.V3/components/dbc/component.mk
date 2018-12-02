#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_COMP_RE_TOOLS

YACC=	yacc
ifdef HOSTTYPE
ifeq ($(HOSTTYPE), FreeBSD)
YACC=	bison
endif
endif

COMPONENT_ADD_INCLUDEDIRS:=src yacclex
COMPONENT_SRCDIRS:=src yacclex
COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
COMPONENT_OBJS = src/dbc_app.o src/dbc.o yacclex/dbc_tokeniser.o yacclex/dbc_parser.o

COMPONENT_EXTRA_CLEAN := $(COMPONENT_PATH)/yacclex/dbc_tokeniser.cpp \
	$(COMPONENT_PATH)/yacclex/dbc_tokeniser.c \
	$(COMPONENT_PATH)/yacclex/dbc_tokeniser.hpp \
	$(COMPONENT_PATH)/yacclex/dbc_parser.hpp \
	$(COMPONENT_PATH)/yacclex/dbc_parser.cpp

src/dbc.o: $(COMPONENT_PATH)/yacclex/dbc_tokeniser.cpp $(COMPONENT_PATH)/yacclex/dbc_parser.hpp

src/dbc_tokeniser.l:

src/dbc_parser.y:

$(COMPONENT_PATH)/yacclex/dbc_tokeniser.cpp : $(COMPONENT_PATH)/src/dbc_tokeniser.l $(COMPONENT_PATH)/yacclex/dbc_parser.hpp
	echo LEX dbc_tokeniser.l
	lex -o $(COMPONENT_PATH)/yacclex/dbc_tokeniser.cpp \
		--header-file=$(COMPONENT_PATH)/yacclex/dbc_tokeniser.hpp \
		$(COMPONENT_PATH)/src/dbc_tokeniser.l

$(COMPONENT_PATH)/yacclex/dbc_parser.hpp : $(COMPONENT_PATH)/src/dbc_parser.y
	echo YACC dbc_parser.y
	$(YACC) -o $(COMPONENT_PATH)/yacclex/dbc_parser.cpp -d $(COMPONENT_PATH)/src/dbc_parser.y

endif
