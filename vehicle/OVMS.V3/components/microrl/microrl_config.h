/*
Microrl library config files
Autor: Eugene Samoylov aka Helius (ghelius@gmail.com)
*/
#ifndef _MICRORL_CONFIG_H_
#define _MICRORL_CONFIG_H_

#define MICRORL_LIB_VER "1.5.1"

/*********** CONFIG SECTION **************/
/*
Command line length, define cmdline buffer size. Set max number of chars + 1,
because last byte of buffer need to contain '\0' - NULL terminator, and
not use for storing inputed char.
If user input chars more then it parametrs-1, chars not added to command line.*/
#define _COMMAND_LINE_LEN 800

/*
Command token number, define max token it command line, if number of token
typed in command line exceed this value, then prints message about it and
command line not to be parced and 'execute' callback will not calls.
Token is word separate by white space, for example 3 token line:
"IRin> set mode test" */
#define _COMMAND_TOKEN_NMB 16

/*
Define you prompt string here. You can use colors escape code, for highlight you prompt,
for example this prompt will green color (if you terminal supports color)*/
#define _PROMPT_DEFAULT "OVMS> "
//#define _PROMPT_DEFAULT "OVMS > "

/*
Define prompt text (without ESC sequence, only text) prompt length, it needs because if you use
ESC sequence, it's not possible detect only text length*/
#define _PROMPT_LEN       6

/*Define it, if you wanna use completion functional, also set completion callback in you code,
now if user press TAB calls 'copmlitetion' callback. If you no need it, you can just set
NULL to callback ptr and do not use it, but for memory saving tune,
if you are not going to use it - disable this define.*/
#define _USE_COMPLETE

/*Define it, if you want to allow quoting command arguments to include spaces.
Depends upon _QUOTED_TOKEN_NMB parameter */
#define _USE_QUOTING

/*
Quoted token number, define max number of tokens allowed to be quoted.  If the
number of quoted tokens typed in the command line exceeds this value, then
prints message about it and the command line is not parsed and 'execute'
callback is not called.
Quoting protects whitespace, for example 2 quoted tokens:
"IRin> set wifi 'Home Net' 'this is a secret'" */
#define _QUOTED_TOKEN_NMB 10

/*Define it, if you wanna use history. It s work's like bash history, and
set stored value to cmdline, if UP and DOWN key pressed. Using history add
memory consuming, depends from _RING_HISTORY_LEN parametr */
#define _USE_HISTORY

/*
History ring buffer length, define static buffer size.
For saving memory, each entered cmdline store to history in ring buffer,
so we can not say, how many line we can store, it depends from cmdline len,
but memory using more effective. We not prefer dinamic memory allocation for
small and embedded devices. Overhead is 2 char on each saved line*/
#define _RING_HISTORY_LEN 256

/*
Size of the buffer used for piecemeal printing of part or all of the command
line.  Allocated on the stack.  Must be at least 16. */
#define _PRINT_BUFFER_LEN 40

/*
Enable Handling terminal ESC sequence. If disabling, then cursor arrow, HOME, END will not work,
use Ctrl+A(B,F,P,N,A,E,H,K,U,C) see README, but decrease code memory.*/
#define _USE_ESC_SEQ

/*
Use sprintf from you standard complier library, but it gives some overhead.
If not defined, use my own number conversion code, it's save about 800 byte of
code size on AVR (avr-gcc build).
Try to build with and without, and compare total code size for tune library.
*/
#define _USE_LIBC_STDIO

/*
Use a single carriage return character to move the cursor to the left margin
rather than moving left by a large number.  This reduces the number of
characters sent to the terminal, but should be left undefined if the terminal
will also simulate a linefeed when it receives the carriage return. */
#define _USE_CARRIAGE_RETURN

/*
Enable 'interrupt signal' callback, if user press Ctrl+C */
#define _USE_CTLR_C

/*
Print prompt at 'microrl_init', if enable, prompt will print at startup,
otherwise first prompt will print after first press Enter in terminal
NOTE!: Enable it, if you call 'microrl_init' after your communication subsystem
already initialize and ready to print message */
//#define _ENABLE_INIT_PROMPT

/*
New line symbol */
#define _ENDL_LF

#if defined(_ENDL_CR)
#define ENDL "\r"
#elif defined(_ENDL_CRLF)
#define ENDL "\r\n"
#elif defined(_ENDL_LF)
#define ENDL "\n"
#elif defined(_ENDL_LFCR)
#define ENDL "\n\r"
#else
#error "You must define new line symbol."
#endif

/********** END CONFIG SECTION ************/


#if _RING_HISTORY_LEN > 256
#error "This history implementation (ring buffer with 1 byte iterator) allow 256 byte buffer size maximum"
#endif

#endif
