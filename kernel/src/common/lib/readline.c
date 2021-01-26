#include <common/lib/readline.h>
#include <hal/drivers/tty.h>

size_t Readline(char *buf, size_t size) {
	if (size == 0) {
		return 0;
	}
	size_t pos = 0;
	while (true) {
		struct HAL_TTY_KeyEvent event;
		HAL_TTY_WaitForNextEvent(&event);
		if ((!event.typeable) || (!event.pressed)) {
			continue;
		}
		if (event.character == '\t') {
			continue;
		} else if (event.character == '\b') {
			if (pos != 0) {
				printf("\b \b");
				pos--;
				buf[pos] = '\0';
			}
		} else if (event.character == '\n') {
			printf("\n");
			buf[pos] = '\0';
			return pos;
		} else {
			if (pos == size - 2) {
				continue;
			}
			printf("%c", event.character);
			buf[pos] = event.character;
			pos++;
		}
	}
}