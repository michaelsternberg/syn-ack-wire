/* Where everything is on the 80x24 screen: measured from the reference screenshot (16 pixels a column, 34 a row). */
#ifndef LINUX_LAYOUT_H
#define LINUX_LAYOUT_H

#define ROW_BAR      0                  /* the title bar */
#define ROW_HEAD     1                  /* column letters; HIVES SIZE SYNDICATE */
#define ROW_GRID     2                  /* the Grid's first row (9 rows: 2-10); the syndicates' lines run from here too */
#define ROW_ICE      10                 /* BLACK ICE AT 11, NULL-ROUTE AT 41 (right of the Grid) */
#define ROW_LAST     11                 /* LAST MOVE */
#define ROW_SEATHEAD 12
#define ROW_SEATS    13                 /* six seats: 13-18 */
#define ROW_LOG      19                 /* three lines: 19-21 */
#define LOG_ROWS     3
#define ROW_INFO     22                 /* the prompt strip: the kit, what you hold ... */
#define ROW_ASK      23                 /* ... and the question */

#define COL_ROWNUM   2                  /* the Grid's row digits */
#define COL_GRID     5                  /* the Grid: 12 cells of CELL_W columns */
#define CELL_W       3
#define COL_HIVE     44                 /* the syndicate panel: letter block (3), size (right-aligned in 3), bar, name, badge */
#define COL_SIZE     47
#define COL_BAR      51
#define BAR_W        8
#define SIZE_MAX     41
#define COL_NAME     60
#define COL_ICE      76

#define COL_TURN     1                  /* a seat: the marker, the name (8), a status character, the cash, the holdings, who */
#define COL_SEAT     3
#define COL_STATUS   11
#define COL_CASH     14                 /* 8 columns, right-aligned: ends at column 21 */
#define CASH_W       8
#define COL_HOLD0    26                 /* hive k's block: columns 26+5k .. 28+5k; its number ends at 28+5k */
#define HOLD_STEP    5
#define COL_WHO      63

#define COL_LOGMARK  1                  /* a log line: [x] then text from column 5 */
#define COL_LOGTEXT  5
#define COL_PROMPT   1

/* the title: the logo from row 1, then three lines and the keys */
#define ROW_REGION   16                 /* REGION, STATUS, HANDLE on this row and the next two */
#define ROW_KEYS     20                 /* TAB, ^S and ? on this row and the next two */
#define COL_LABEL    22
#define COL_VALUE    31
#define COL_KEYS     27
#define COL_HINTS    39                 /* ON / OFF after MOVE HINTS */
#define TITLE_SHIFT  0                  /* the doubled logo's first column, in cells */

/* the instruction panel: framed, over the screen, rows 1-20 */
#define HELP_ROW     1
#define HELP_COL     10
#define HELP_W       60
#define HELP_H       21

#endif
