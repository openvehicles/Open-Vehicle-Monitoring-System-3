This is a temporary directory used for yacc and lex intermediary
files.

The component.mk controls the compilation. It does:

  $ yacc -o $(COMPONENT_PATH)/yacclex/dbc_parser.cpp
         -d $(COMPONENT_PATH)/src/dbc_parser.y

  The expectation here is that the ‘-o’ produces dbc_parser.cpp
  and dbs_parser.hpp

  $ lex -o $(COMPONENT_PATH)/yacclex/dbc_tokeniser.c
    $(COMPONENT_PATH)/src/dbc_tokeniser.l

  This #include the dbc_parser.hpp and produced dbc_tokeniser.c

  Then dbc_tokeniser.c and dbc_parser.cpp are compiled as normal

We use ‘yacclex’ as a temporary path, to keep these auto-generated code
files separate.

The behaviour of bison is that ‘-o’ outputs both .hpp and .cpp files
(from the ‘-o’ directive). If think byacc may behave differently. We
need both the header and code files, and they need to be named .hpp
and .cpp as they need to produce c++ code. The dbc_tokeniser is in C,
as Lex only really supports C and we can “extern C” the stuff we need
when we access it (there is not much).
