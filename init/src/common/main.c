#include <common/cpl1syscalls.h>

static const char msg[] = "\033[92m\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
						  "\t\t\t\t\t\t\t\t\t\t\t  /$$$$$$  /$$$$$$$  /$$         /$$  \n"
						  "\t\t\t\t\t\t\t\t\t\t\t /$$__  $$| $$__  $$| $$       /$$$$  \n"
						  "\t\t\t\t\t\t\t\t\t\t\t| $$  \\__/| $$  \\ $$| $$      |_  $$  \n"
						  "\t\t\t\t\t\t\t\t\t\t\t| $$      | $$$$$$$/| $$ /$$$$$$| $$  \n"
						  "\t\t\t\t\t\t\t\t\t\t\t| $$      | $$____/ | $$|______/| $$  \n"
						  "\t\t\t\t\t\t\t\t\t\t\t| $$    $$| $$      | $$        | $$  \n"
						  "\t\t\t\t\t\t\t\t\t\t\t|  $$$$$$/| $$      | $$$$$$$$ /$$$$$$\n"
						  "\t\t\t\t\t\t\t\t\t\t\t \\______/ |__/      |________/|______/\n"
						  "\n"
						  "\t\t\t\t\t\t\t\t\t\t\t      Booting up. Please wait...\n"
						  "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\033[39m";

int main() {
	int ttyFd = CPL1_SyscallOpen("/dev/tty0", 0);
	if (ttyFd < 0) {
		return -1;
	}
	int status = CPL1_SyscallWrite(ttyFd, msg, sizeof(msg));
	if (status != sizeof(msg)) {
		return -1;
	}
	status = CPL1_SyscallClose(ttyFd);
	if (status != 0) {
		return -1;
	}
	return 0;
}