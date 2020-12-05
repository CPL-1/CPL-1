#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/tty.h>
#include <arch/i686/init/stivale.h>
#include <common/misc/font.h>
#include <hal/drivers/tty.h>
#include <hal/memory/virt.h>

UINT16 i686_TTY_Y, i686_TTY_X;
UINT16 i686_TTY_YSize, i686_TTY_XSize;
UINT32 *i686_TTY_framebuffer;
struct i686_Stivale_FramebufferInfo i686_TTY_fb;
bool i686_TTY_isFramebufferInitialized = false;

void i686_TTY_ReportFramebufferError(const char *msg, USIZE size) {
	memset((void *)(HAL_VirtualMM_KernelMappingBase + 0xb8000), 0, 4000);
	for (USIZE i = 0; i < size; ++i) {
		((char *)(HAL_VirtualMM_KernelMappingBase + 0xb8000))[2 * i] = msg[i];
		((char *)(HAL_VirtualMM_KernelMappingBase + 0xb8000))[2 * i + 1] = 0x0f;
	}
	while (true) {
		ASM volatile("nop");
	}
};

void i686_TTY_PutCharacterAt(UNUSED UINT16 x, UNUSED UINT16 y, UNUSED UINT32 color, UNUSED char c) {
}

void i686_TTY_Initialize() {
	i686_TTY_X = 0;
	i686_TTY_Y = 0;
	struct i686_Stivale_FramebufferInfo fb;
	if (!i686_Stivale_GetFramebufferInfo(&fb)) {
		const char msg[] = "[ FAIL ] i686 Kernel Init: Failed to find framebuffer. Please "
						   "make sure that VBE mode is supported, as it is "
						   "required to for CPL-1 kernel to boot";
		i686_TTY_ReportFramebufferError(msg, ARR_SIZE(msg));
	}
	i686_TTY_XSize = fb.framebufferWidth / FONT_fontWidth;
	i686_TTY_YSize = fb.framebufferHeight / FONT_fontHeight;
	i686_TTY_isFramebufferInitialized = true;
}

void HAL_TTY_Flush() {
}

void HAL_TTY_PrintCharacter(char c) {
	i686_Ports_WriteByte(0xe9, c);
}

void HAL_TTY_SetColor(UNUSED UINT8 color) {
}
