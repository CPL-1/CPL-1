#include <common/core/proc/mutex.h>
#include <common/lib/readline.h>
#include <hal/drivers/tty.h>

static struct Mutex m_mutex;

size_t ReadLine(char *buf, size_t size) {
	if (size == 0) {
		return 0;
	} else if (size == 1) {
		buf[0] = '\0';
		return 0;
	}
	Mutex_Lock(&m_mutex);
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
			buf[pos] = '\n';
			buf[pos + 1] = '\0';
			Mutex_Unlock(&m_mutex);
			return pos + 1;
		} else {
			if (pos >= size - 2) {
				continue;
			}
			printf("%c", event.character);
			buf[pos] = event.character;
			pos++;
		}
	}
}

void ReadLine_Initialize() {
	Mutex_Initialize(&m_mutex);
}