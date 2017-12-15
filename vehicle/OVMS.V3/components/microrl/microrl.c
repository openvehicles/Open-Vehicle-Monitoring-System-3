/*
Author: Samoylov Eugene aka Helius (ghelius@gmail.com)
BUGS and TODO:
-- add echo_off feature
-- rewrite history for use more than 256 byte buffer
*/

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "microrl.h"
#ifdef _USE_LIBC_STDIO
#include <stdio.h>
#endif

//#define DBG(...) fprintf(stderr, "\033[33m");fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\033[0m");

char * prompt_default = _PROMPT_DEFAULT;

#ifdef _USE_HISTORY

#ifdef _HISTORY_DEBUG
//*****************************************************************************
// print buffer content on screen
static void print_hist (ring_history_t * pThis)
{
	printf ("\n");
	for (int i = 0; i < _RING_HISTORY_LEN; i++) {
		if (i == pThis->begin)
			printf ("b");
		else
			printf (" ");
	}
	printf ("\n");
	for (int i = 0; i < _RING_HISTORY_LEN; i++) {
		if (isalpha(pThis->ring_buf[i]))
			printf ("%c", pThis->ring_buf[i]);
		else
			printf ("%d", pThis->ring_buf[i]);
	}
	printf ("\n");
	for (int i = 0; i < _RING_HISTORY_LEN; i++) {
		if (i == pThis->end)
			printf ("e");
		else
			printf (" ");
	}
	printf ("\n");
}
#endif

//*****************************************************************************
// remove older message from ring buffer
static void hist_erase_older (ring_history_t * pThis)
{
	int new_pos = pThis->begin + pThis->ring_buf [pThis->begin] + 1;
	if (new_pos >= _RING_HISTORY_LEN)
		new_pos = new_pos - _RING_HISTORY_LEN;

	pThis->begin = new_pos;
}

//*****************************************************************************
// check space for new line, remove older while not space
static int hist_is_space_for_new (ring_history_t * pThis, int len)
{
	if (pThis->ring_buf [pThis->begin] == 0)
		return true;
	if (pThis->end >= pThis->begin) {
		if (_RING_HISTORY_LEN - pThis->end + pThis->begin - 1 > len)
			return true;
	}	else {
		if (pThis->begin - pThis->end - 1> len)
			return true;
	}
	return false;
}

//*****************************************************************************
// put line to ring buffer
static void hist_save_line (ring_history_t * pThis, char * line, int len)
{
	if (len > _RING_HISTORY_LEN - 2)
		return;
	while (!hist_is_space_for_new (pThis, len)) {
		hist_erase_older (pThis);
	}
	// if it's first line
	if (pThis->ring_buf [pThis->begin] == 0)
		pThis->ring_buf [pThis->begin] = len;

	// store line
	if (len < _RING_HISTORY_LEN-pThis->end-1)
		memcpy (pThis->ring_buf + pThis->end + 1, line, len);
	else {
		int part_len = _RING_HISTORY_LEN-pThis->end-1;
		memcpy (pThis->ring_buf + pThis->end + 1, line, part_len);
		memcpy (pThis->ring_buf, line + part_len, len - part_len);
	}
	pThis->ring_buf [pThis->end] = len;
	pThis->end = pThis->end + len + 1;
	if (pThis->end >= _RING_HISTORY_LEN)
		pThis->end -= _RING_HISTORY_LEN;
	pThis->ring_buf [pThis->end] = 0;
	pThis->cur = 0;
#ifdef _HISTORY_DEBUG
	print_hist (pThis);
#endif
}

//*****************************************************************************
// copy saved line to 'line' and return size of line
static int hist_restore_line (ring_history_t * pThis, char * line, int dir)
{
	int cnt = 0;
	// count history record
	int header = pThis->begin;
	while (pThis->ring_buf [header] != 0) {
		header += pThis->ring_buf [header] + 1;
		if (header >= _RING_HISTORY_LEN)
			header -= _RING_HISTORY_LEN;
		cnt++;
	}

	if (dir == _HIST_UP) {
		if (cnt >= pThis->cur) {
			int header = pThis->begin;
			int j = 0;
			// found record for 'pThis->cur' index
			while ((pThis->ring_buf [header] != 0) && (cnt - j -1 != pThis->cur)) {
				header += pThis->ring_buf [header] + 1;
				if (header >= _RING_HISTORY_LEN)
					header -= _RING_HISTORY_LEN;
				j++;
			}
			if (pThis->ring_buf[header]) {
					pThis->cur++;
				// obtain saved line
				if (pThis->ring_buf [header] + header < _RING_HISTORY_LEN) {
					memset (line, 0, _COMMAND_LINE_LEN);
					memcpy (line, pThis->ring_buf + header + 1, pThis->ring_buf[header]);
				} else {
					int part0 = _RING_HISTORY_LEN - header - 1;
					memset (line, 0, _COMMAND_LINE_LEN);
					memcpy (line, pThis->ring_buf + header + 1, part0);
					memcpy (line + part0, pThis->ring_buf, pThis->ring_buf[header] - part0);
				}
				return pThis->ring_buf[header];
			}
		}
	} else {
		if (pThis->cur > 0) {
				pThis->cur--;
			int header = pThis->begin;
			int j = 0;

			while ((pThis->ring_buf [header] != 0) && (cnt - j != pThis->cur)) {
				header += pThis->ring_buf [header] + 1;
				if (header >= _RING_HISTORY_LEN)
					header -= _RING_HISTORY_LEN;
				j++;
			}
			if (pThis->ring_buf [header] + header < _RING_HISTORY_LEN) {
				memcpy (line, pThis->ring_buf + header + 1, pThis->ring_buf[header]);
			} else {
				int part0 = _RING_HISTORY_LEN - header - 1;
				memcpy (line, pThis->ring_buf + header + 1, part0);
				memcpy (line + part0, pThis->ring_buf, pThis->ring_buf[header] - part0);
			}
			return pThis->ring_buf[header];
		} else {
			/* empty line */
			return 0;
		}
	}
	return -1;
}
#endif








#ifdef _USE_QUOTING
//*****************************************************************************
// restore end quote marks in cmdline
static void restore (microrl_t * pThis)
{
	int iq;
	for (iq = 0; iq < _QUOTED_TOKEN_NMB; ++iq) {
		if (pThis->quotes[iq].end == 0)
			break;
		*pThis->quotes[iq].end = *pThis->quotes[iq].begin;
		pThis->quotes[iq].begin = 0;
		pThis->quotes[iq].end = 0;
	}
}
#endif

//*****************************************************************************
// split cmdline to tkn array and return nmb of token
static int split (microrl_t * pThis, int limit, char const ** tkn_arr)
{
	int i = 0;
	int ind = 0;
#ifdef _USE_QUOTING
	int iq = 0;
	char quote = 0;
	for (iq = 0; iq < _QUOTED_TOKEN_NMB; ++iq) {
		pThis->quotes[iq].begin = 0;
		pThis->quotes[iq].end = 0;
	}
	iq = 0;
#endif
	while (1) {
		// go to the first NOT whitespace (not zerro for us)
		while ((pThis->cmdline [ind] == '\0') && (ind < limit)) {
			ind++;
		}
		if (!(ind < limit)) return i;
#ifdef _USE_QUOTING
		if (pThis->cmdline [ind] == '\'' || pThis->cmdline [ind] == '"') {
			if (iq >= _QUOTED_TOKEN_NMB) {
				restore (pThis);
				return -1;
			}
			quote = pThis->cmdline [ind];
			pThis->quotes[iq].begin = pThis->cmdline + ind;
			ind++;
		}
#endif
		tkn_arr[i++] = pThis->cmdline + ind;
		if (i >= _COMMAND_TOKEN_NMB) {
#ifdef _USE_QUOTING
			restore (pThis);
#endif
			return -1;
		}
		// go to the first whitespace (zerro for us)
		while (ind < limit) {
			if (pThis->cmdline [ind] == '\0') {
#ifdef _USE_QUOTING
				if (!quote)
#endif
					break;
#ifdef _USE_QUOTING
				pThis->cmdline [ind] = ' ';
			} else if (pThis->cmdline [ind] == quote) {
				if (pThis->cmdline [ind + 1] != '\0') {
					restore (pThis);
					return -1;
				}
				quote = 0;
				pThis->quotes[iq++].end = pThis->cmdline + ind;
				pThis->cmdline [ind++] = '\0';
				break;
#endif
			}
			ind++;
		}
		if (!(ind < limit)) {
#ifdef _USE_QUOTING
			if (quote) {
				restore (pThis);
				return -1;
			}
#endif
			return i;
		}
	}
	return i;
}


//*****************************************************************************
inline static void print_prompt (microrl_t * pThis)
{
	pThis->print (pThis, pThis->prompt_str);
}

//*****************************************************************************
inline static void terminal_backspace (microrl_t * pThis)
{
		pThis->print (pThis, "\033[D \033[D");
}

//*****************************************************************************
inline static void terminal_newline (microrl_t * pThis)
{
	pThis->print (pThis, ENDL);
}

#ifndef _USE_LIBC_STDIO
//*****************************************************************************
// convert 16 bit value to string
// 0 value not supported!!! just make empty string
// Returns pointer to a buffer tail
static char *u16bit_to_str (unsigned int nmb, char * buf)
{
	char tmp_str [6] = {0,};
	int i = 0, j;
	if (nmb <= 0xFFFF) {
		while (nmb > 0) {
			tmp_str[i++] = (nmb % 10) + '0';
			nmb /=10;
		}
		for (j = 0; j < i; ++j)
			*(buf++) = tmp_str [i-j-1];
	}
	*buf = '\0';
	return buf;
}
#endif


//*****************************************************************************
// set cursor at position from begin cmdline (after prompt) + offset
static void terminal_move_cursor (microrl_t * pThis, int offset)
{
	char str[16] = {0,};
#ifdef _USE_LIBC_STDIO
	if (offset > 0) {
		snprintf (str, 16, "\033[%dC", offset);
	} else if (offset < 0) {
		snprintf (str, 16, "\033[%dD", -(offset));
	}
#else
	char *endstr;
	strcpy (str, "\033[");
	if (offset > 0) {
		endstr = u16bit_to_str (offset, str+2);
		strcpy (endstr, "C");
	} else if (offset < 0) {
		endstr = u16bit_to_str (-(offset), str+2);
		strcpy (endstr, "D");
	} else
		return;
#endif
	pThis->print (pThis, str);
}

//*****************************************************************************
static void terminal_reset_cursor (microrl_t * pThis)
{
	char str[16];
#ifdef _USE_LIBC_STDIO
	snprintf (str, 16, "\033[%dD\033[%dC", \
						_COMMAND_LINE_LEN + _PROMPT_LEN + 2, _PROMPT_LEN);

#else
	char *endstr;
	strcpy (str, "\033[");
	endstr = u16bit_to_str ( _COMMAND_LINE_LEN + _PROMPT_LEN + 2,str+2);
	strcpy (endstr, "D\033["); endstr += 3;
	endstr = u16bit_to_str (_PROMPT_LEN, endstr);
	strcpy (endstr, "C");
#endif
	pThis->print (pThis, str);
}

//*****************************************************************************
// print cmdline to screen, replace '\0' to wihitespace
static void terminal_print_line (microrl_t * pThis, int pos, int cursor)
{
	pThis->print (pThis, "\033[K");    // delete all from cursor to end

	char nch [] = {0,0};
	int i;
	for (i = pos; i < pThis->cmdlen; i++) {
		nch [0] = pThis->cmdline [i];
		if (nch[0] == '\0')
			nch[0] = ' ';
		pThis->print (pThis, nch);
	}

	terminal_reset_cursor (pThis);
	terminal_move_cursor (pThis, cursor);
}

//*****************************************************************************
void microrl_init (microrl_t * pThis, void (*print) (microrl_t*, const char *))
{
	memset(pThis, 0, sizeof(microrl_t));
	pThis->prompt_str = prompt_default;
	pThis->print = print;
#ifdef _ENABLE_INIT_PROMPT
	print_prompt (pThis);
#endif
}

//*****************************************************************************
void microrl_set_complete_callback (microrl_t * pThis, char ** (*get_completion)(microrl_t*, int, const char* const*))
{
	pThis->get_completion = get_completion;
}

//*****************************************************************************
void microrl_set_execute_callback (microrl_t * pThis, int (*execute)(microrl_t*, int, const char* const*))
{
	pThis->execute = execute;
}
#ifdef _USE_CTLR_C
//*****************************************************************************
void microrl_set_sigint_callback (microrl_t * pThis, void (*sigintf)(microrl_t*))
{
	pThis->sigint = sigintf;
}
#endif

#ifdef _USE_HISTORY
static void hist_search (microrl_t * pThis, int dir)
{
	int len = hist_restore_line (&pThis->ring_hist, pThis->cmdline, dir);
	if (len >= 0) {
		pThis->cmdline[len] = '\0';
		pThis->cursor = pThis->cmdlen = len;
		terminal_reset_cursor (pThis);
		terminal_print_line (pThis, 0, pThis->cursor);
	}
}
#endif

#ifdef _USE_ESC_SEQ
//*****************************************************************************
// handling escape sequences
static int escape_process (microrl_t * pThis, char ch)
{
	if (ch == '[') {
		pThis->escape_seq = _ESC_BRACKET;
		return 0;
	} else if (pThis->escape_seq == _ESC_BRACKET) {
		if (ch == 'A') {
#ifdef _USE_HISTORY
			hist_search (pThis, _HIST_UP);
#endif
			return 1;
		} else if (ch == 'B') {
#ifdef _USE_HISTORY
			hist_search (pThis, _HIST_DOWN);
#endif
			return 1;
		} else if (ch == 'C') {
			if (pThis->cursor < pThis->cmdlen) {
				terminal_move_cursor (pThis, 1);
				pThis->cursor++;
			}
			return 1;
		} else if (ch == 'D') {
			if (pThis->cursor > 0) {
				terminal_move_cursor (pThis, -1);
				pThis->cursor--;
			}
			return 1;
		} else if (ch == '7') {
			pThis->escape_seq = _ESC_HOME;
			return 0;
		} else if (ch == '8') {
			pThis->escape_seq = _ESC_END;
			return 0;
		}
	} else if (ch == '~') {
		if (pThis->escape_seq == _ESC_HOME) {
			terminal_reset_cursor (pThis);
			pThis->cursor = 0;
			return 1;
		} else if (pThis->escape_seq == _ESC_END) {
			terminal_move_cursor (pThis, pThis->cmdlen-pThis->cursor);
			pThis->cursor = pThis->cmdlen;
			return 1;
		}
	}

	/* unknown escape sequence, stop */
	return 1;
}
#endif

//*****************************************************************************
// insert len char of text at cursor position
static int microrl_insert_text (microrl_t * pThis, char * text, int len)
{
	int i;
	if (pThis->cmdlen + len < _COMMAND_LINE_LEN) {
		memmove (pThis->cmdline + pThis->cursor + len,
						 pThis->cmdline + pThis->cursor,
						 pThis->cmdlen - pThis->cursor);
		for (i = 0; i < len; i++) {
			pThis->cmdline [pThis->cursor + i] = text [i];
			if (pThis->cmdline [pThis->cursor + i] == ' ') {
				pThis->cmdline [pThis->cursor + i] = 0;
			}
		}
		pThis->cursor += len;
		pThis->cmdlen += len;
		pThis->cmdline [pThis->cmdlen] = '\0';
		return true;
	}
	return false;
}

//*****************************************************************************
// remove one char at cursor
static void microrl_backspace (microrl_t * pThis)
{
	if (pThis->cursor > 0) {
		terminal_backspace (pThis);
		memmove (pThis->cmdline + pThis->cursor-1,
						 pThis->cmdline + pThis->cursor,
						 pThis->cmdlen-pThis->cursor+1);
		pThis->cursor--;
		pThis->cmdline [pThis->cmdlen] = '\0';
		pThis->cmdlen--;
	}
}

//*****************************************************************************
// remove one char forward at cursor
static void microrl_delete (microrl_t * pThis)
{
	memmove (pThis->cmdline + pThis->cursor,
					 pThis->cmdline + pThis->cursor+1,
					 pThis->cmdlen-pThis->cursor+1);
		pThis->cmdline [pThis->cmdlen] = '\0';
		pThis->cmdlen--;
}

#ifdef _USE_COMPLETE

//*****************************************************************************
static int common_len (char ** arr)
{
	int i;
	int j;
	char *shortest = arr[0];
	int shortlen = strlen(shortest);

	for (i = 0; arr[i] != NULL; ++i)
		if (strlen(arr[i]) < shortlen) {
			shortest = arr[i];
			shortlen = strlen(shortest);
		}

	for (i = 0; i < shortlen; ++i)
		for (j = 0; arr[j] != 0; ++j)
			if (shortest[i] != arr[j][i])
				return i;

	return i;
}

//*****************************************************************************
static void microrl_get_complite (microrl_t * pThis)
{
	char const * tkn_arr[_COMMAND_TOKEN_NMB];
	char ** compl_token;

	if (pThis->get_completion == NULL) // callback was not set
		return;

	int status = split (pThis, pThis->cursor, tkn_arr);
	if (status < 0)
		return;
	if (pThis->cmdline[pThis->cursor-1] == '\0')
		tkn_arr[status++] = "";
	compl_token = pThis->get_completion (pThis, status, tkn_arr);
#ifdef _USE_QUOTING
	restore (pThis);
#endif
	if (compl_token[0] != NULL) {
		int i = 0;
		int len;

		if (compl_token[1] == NULL) {
			len = strlen (compl_token[0]);
		} else {
			len = common_len (compl_token);
			terminal_newline (pThis);
			while (compl_token [i] != NULL) {
				pThis->print (pThis, compl_token[i]);
				pThis->print (pThis, " ");
				i++;
			}
			terminal_newline (pThis);
			print_prompt (pThis);
		}

		if (len) {
			microrl_insert_text (pThis, compl_token[0] + strlen(tkn_arr[status-1]),
																	len - strlen(tkn_arr[status-1]));
			if (compl_token[1] == NULL)
				microrl_insert_text (pThis, " ", 1);
		}
		terminal_reset_cursor (pThis);
		terminal_print_line (pThis, 0, pThis->cursor);
	}
}
#endif

//*****************************************************************************
void new_line_handler(microrl_t * pThis){
	char const * tkn_arr [_COMMAND_TOKEN_NMB];
	int status;

	terminal_newline (pThis);
#ifdef _USE_HISTORY
	if (pThis->cmdlen > 0)
		hist_save_line (&pThis->ring_hist, pThis->cmdline, pThis->cmdlen);
#endif
	status = split (pThis, pThis->cmdlen, tkn_arr);
	if (status == -1){
		//          pThis->print ("ERROR: Max token amount exseed\n");
#ifdef _USE_QUOTING
		pThis->print (pThis, "ERROR:too many tokens or invalid quoting");
#else
		pThis->print (pThis, "ERROR:too many tokens");
#endif
		pThis->print (pThis, ENDL);
	}
	if ((status > 0) && (pThis->execute != NULL))
		pThis->execute (pThis, status, tkn_arr);
	print_prompt (pThis);
	pThis->cmdlen = 0;
	pThis->cursor = 0;
	memset(pThis->cmdline, 0, _COMMAND_LINE_LEN);
#ifdef _USE_HISTORY
	pThis->ring_hist.cur = 0;
#endif
}

//*****************************************************************************

void microrl_insert_char (microrl_t * pThis, int ch)
{
#ifdef _USE_ESC_SEQ
	if (pThis->escape) {
		if (escape_process(pThis, ch))
			pThis->escape = 0;
	} else {
#endif
		switch (ch) {
			//-----------------------------------------------------
#ifdef _ENDL_CR
			case KEY_CR:
				new_line_handler(pThis);
			break;
			case KEY_LF:
			break;
#elif defined(_ENDL_CRLF)
			case KEY_CR:
				pThis->tmpch = KEY_CR;
			break;
			case KEY_LF:
			if (pThis->tmpch == KEY_CR)
				new_line_handler(pThis);
			break;
#elif defined(_ENDL_LFCR)
			case KEY_LF:
				pThis->tmpch = KEY_LF;
			break;
			case KEY_CR:
			if (pThis->tmpch == KEY_LF)
				new_line_handler(pThis);
			break;
#else
			case KEY_CR:
			break;
			case KEY_LF:
				new_line_handler(pThis);
			break;
#endif
			//-----------------------------------------------------
#ifdef _USE_COMPLETE
			case KEY_HT:
				microrl_get_complite (pThis);
			break;
#endif
			//-----------------------------------------------------
			case KEY_ESC:
#ifdef _USE_ESC_SEQ
				pThis->escape = 1;
#endif
			break;
			//-----------------------------------------------------
			case KEY_NAK: // ^U
					while (pThis->cursor > 0) {
					microrl_backspace (pThis);
				}
				terminal_print_line (pThis, 0, pThis->cursor);
			break;
			//-----------------------------------------------------
			case KEY_VT:  // ^K
				pThis->print (pThis,"\033[K");
				pThis->cmdlen = pThis->cursor;
			break;
			//-----------------------------------------------------
			case KEY_ENQ: // ^E
				terminal_move_cursor (pThis, pThis->cmdlen-pThis->cursor);
				pThis->cursor = pThis->cmdlen;
			break;
			//-----------------------------------------------------
			case KEY_SOH: // ^A
				terminal_reset_cursor (pThis);
				pThis->cursor = 0;
			break;
			//-----------------------------------------------------
			case KEY_ACK: // ^F
			if (pThis->cursor < pThis->cmdlen) {
				terminal_move_cursor (pThis, 1);
				pThis->cursor++;
			}
			break;
			//-----------------------------------------------------
			case KEY_STX: // ^B
			if (pThis->cursor) {
				terminal_move_cursor (pThis, -1);
				pThis->cursor--;
			}
			break;
			//-----------------------------------------------------
			case KEY_DLE: //^P
#ifdef _USE_HISTORY
			hist_search (pThis, _HIST_UP);
#endif
			break;
			//-----------------------------------------------------
			case KEY_SO: //^N
#ifdef _USE_HISTORY
			hist_search (pThis, _HIST_DOWN);
#endif
			break;
			//-----------------------------------------------------
			case KEY_DEL: // Backspace
			case KEY_BS: // ^H
				microrl_backspace (pThis);
				terminal_print_line (pThis, pThis->cursor, pThis->cursor);
			break;
			//-----------------------------------------------------
			case KEY_EOT: // ^D
				microrl_delete (pThis);
				terminal_print_line (pThis, pThis->cursor, pThis->cursor);
			break;
			//-----------------------------------------------------
			case KEY_DC2: // ^R
				terminal_newline (pThis);
				print_prompt (pThis);
				terminal_reset_cursor (pThis);
				terminal_print_line (pThis, 0, pThis->cursor);
			break;
			//-----------------------------------------------------
#ifdef _USE_CTLR_C
			case KEY_ETX:
			if (pThis->sigint != NULL)
				pThis->sigint(pThis);
			break;
#endif
			//-----------------------------------------------------
			default:
			if (((ch == ' ') && (pThis->cmdlen == 0)) || IS_CONTROL_CHAR(ch))
				break;
			if (microrl_insert_text (pThis, (char*)&ch, 1))
				terminal_print_line (pThis, pThis->cursor-1, pThis->cursor);

			break;
		}
#ifdef _USE_ESC_SEQ
	}
#endif
}
