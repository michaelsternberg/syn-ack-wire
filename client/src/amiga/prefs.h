/* The player's handle and, if they want another server, its address, kept in S:SynAckWire.prefs as lines:
 *   HANDLE=NAME
 *   SERVER=TCP://host:port/        (an N: in front is accepted; without the line the built-in server is dialled)
 * The format is pure and tested on the host; the file itself is prefs_io.c's. */
#ifndef AMIGA_PREFS_H
#define AMIGA_PREFS_H

#define PREFS_FILE "S:SynAckWire.prefs"
#define HANDLE_MAX 8
#define SERVER_MAX 60                   /* longer addresses are ignored, not cut */
#define PREFS_FILE_MAX 115              /* the longest file prefs_format writes: HANDLE= 16, SERVER= 68, VIEW=TABLE 11, HINTS=OFF 10, SOUND=OFF 10 */

/* The settings that are kept, as lines VIEW=TABLE|CARDS, HINTS=ON|OFF and SOUND=ON|OFF; a missing line, or a value that is
 * neither of the two, leaves the default (cards, hints off, sound on). */
struct prefs_set { unsigned char view, hints, sound; };     /* view 1 = the table */
void prefs_defaults(struct prefs_set *s);
void prefs_settings(const char *file, unsigned char len, struct prefs_set *s);

/* The handle from the file's `len` bytes, upcased, only what the title lets a player type (above a space, up to Z),
 * at most HANDLE_MAX, NUL-ended in `handle` (9 bytes). Returns its length; 0 for no HANDLE line. */
unsigned char prefs_parse(const char *file, unsigned char len, char *handle);

/* The server address from the file: what follows SERVER= up to the line's end, minus a leading N:, NUL-ended in `url`
 * (SERVER_MAX + 1 bytes). Returns its length; 0 (and an empty url) if there is no such line, it does not start with TCP://,
 * or it is too long. */
unsigned char prefs_server(const char *file, unsigned char len, char *url);

/* The file for `handle` (NUL-ended, filtered the same way), `server` ("" for none: no SERVER line) and the settings, in `out`
 * (PREFS_FILE_MAX bytes); returns its length. */
unsigned char prefs_format(const char *handle, const char *server, const struct prefs_set *set, char *out);

#endif
