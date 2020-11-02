#ifndef __VGA_TEXTINCLUDED__
#define __VGA_TEXTINCLUDED__

#include <utils.h>

void vga_init();
void vga_putc_no_cursor_update(char c);
void vga_puts(const char *str);
void vga_update_cursor();

#endif