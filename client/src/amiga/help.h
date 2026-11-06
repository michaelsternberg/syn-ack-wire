/* The instruction pages (help_tab.h, generated from help/amiga/screen by tools/help_amiga.py), drawn over the whole
 * screen in Topaz 8: 40 columns by 24 rows, a heading and footer in reverse, the QR page as pixels. */
#ifndef AMIGA_HELP_H
#define AMIGA_HELP_H

unsigned char help_pages(void);
void help_draw(unsigned char page);     /* page 1..help_pages(): clears the screen and draws it */

#endif
