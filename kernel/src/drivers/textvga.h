#ifndef __VGA_TEXTINCLUDED__
#define __VGA_TEXTINCLUDED__

#include <utils/utils.h>

void vga_init();
void vga_clear_screen();
void vga_putc_no_cursor_update(char c);
void vga_set_color(uint8_t color);
void vga_puts(const char *str);
void vga_update_cursor();

#endif
