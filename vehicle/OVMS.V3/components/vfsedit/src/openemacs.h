#ifndef _OPENEMACS_H_
#define _OPENEMACS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define EDITOR_KEY_MAX_SEQUENCE 16

struct editor_state
  {
  int cursor_x, cursor_y;                 // Cursor x and y position in characters
  int desired_cursor_x;                   // Cursor x which user wants if sufficient characters on the line
  int row_offset;                         // Offset of row displayed.
  int column_offset;                      // Offset of column displayed.
  int screen_rows;                        // Number of rows that we can show
  int screen_columns;                     // Number of columns that we can show
  int number_of_rows;                     // Number of rows
  int key_pos;                            // Current position in key buffer
  char key_buf[EDITOR_KEY_MAX_SEQUENCE];  // Key buffer
  int key_quit_confirmations_left;        // Quit confirmations left
  int key_previous;                       // Previous key
  bool editor_completed;                  // TRUE if editor has completed
  void* editor_userdata;                  // General user data
  size_t (*write) (struct editor_state* E, const char *buf, size_t nbyte);
  struct editor_row *row;                 // Rows
  bool dirty;                             // File modified but not saved.
  char *filename;                         // Currently open filename
  char *status_message;
  time_t status_message_last_update;
  int number_of_cut_buffer_lines;
  char **cut_buffer_lines;
  bool has_open_cut_buffer;
  struct editor_syntax *syntax_highlight_mode; // Current syntax highlight, or NULL.
  };

void editor_init(struct editor_state* E, size_t (*write) (struct editor_state* E, const char *buf, size_t nbyte), void* userdata);
bool editor_open(struct editor_state* E, char const *filename);
void editor_refresh_screen(struct editor_state* E);
void editor_free(struct editor_state* E);
bool editor_process_keypress(struct editor_state* E, int k);

#ifdef __cplusplus
}
#endif

#endif
