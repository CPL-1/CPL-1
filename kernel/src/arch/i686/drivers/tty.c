#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/tty.h>
#include <arch/i686/init/stivale.h>
#include <common/core/memory/heap.h>
#include <common/lib/kmsg.h>
#include <common/misc/font.h>
#include <hal/drivers/tty.h>
#include <hal/memory/virt.h>

static uint16_t i686_TTY_Y, i686_TTY_X;
static uint16_t i686_TTY_YSize, i686_TTY_XSize;
static uint32_t i686_TTY_framebuffer, i686_TTY_copyFramebuffer;
static struct i686_Stivale_FramebufferInfo i686_TTY_fbInfo;
static bool i686_TTY_isFramebufferInitialized = false;
static uint32_t i686_TTY_Color;
static uint32_t i686_TTY_TabSize = 4;

static void i686_TTY_ReportFramebufferError(const char *msg, size_t size) {
	memset((void *)(HAL_VirtualMM_KernelMappingBase + 0xb8000), 0, 4000);
	for (size_t i = 0; i < size; ++i) {
		((char *)(HAL_VirtualMM_KernelMappingBase + 0xb8000))[2 * i] = msg[i];
		((char *)(HAL_VirtualMM_KernelMappingBase + 0xb8000))[2 * i + 1] = 0x0f;
	}
	while (true) {
		ASM volatile("nop");
	}
};

static void i686_TTY_PutCharacterAt(uint16_t x, uint16_t y, uint32_t color, char c) {
	c -= FONT_firstPrintableChar;
	for (uint16_t charY = 0; charY < FONT_fontHeight; ++charY) {
		for (uint16_t charX = 0; charX < FONT_fontWidth; ++charX) {
			size_t bitmapOffset = c * FONT_fontHeight * FONT_fontWidth / 8;
			size_t bitOffset = charY * FONT_fontWidth + charX;
			size_t byteOffset = bitOffset / 8;
			size_t bitOffsetModule = bitOffset % 8;
			bool drawPixel = (FONT_bitmap[bitmapOffset + byteOffset] & (1 << (FONT_fontWidth - bitOffsetModule))) != 0;
			if (drawPixel) {
				uint16_t screenX = x * FONT_fontWidth + charX;
				uint16_t screenY = y * FONT_fontHeight + charY;
				uint32_t framebufferAddr =
					i686_TTY_framebuffer + screenY * i686_TTY_fbInfo.framebufferPitch + screenX * 4;
				uint32_t copyFramebufferAddr =
					i686_TTY_copyFramebuffer + screenY * i686_TTY_fbInfo.framebufferPitch + screenX * 4;
				*(uint32_t *)framebufferAddr = *(uint32_t *)copyFramebufferAddr = color;
			}
		}
	}
}

static void i686_TTY_Scroll() {
	for (size_t y = 0; y < i686_TTY_YSize - 1U; ++y) {
		for (size_t screenY = y * FONT_fontHeight; screenY < (y + 1) * FONT_fontHeight; ++screenY) {
			uint32_t scanlineAddr = i686_TTY_framebuffer + screenY * i686_TTY_fbInfo.framebufferPitch;
			uint32_t copyFramebufferNewAddr = i686_TTY_copyFramebuffer + screenY * i686_TTY_fbInfo.framebufferPitch;
			uint32_t copyFramebufferAddr =
				i686_TTY_copyFramebuffer + (screenY + FONT_fontHeight) * i686_TTY_fbInfo.framebufferPitch;
			memcpy((void *)scanlineAddr, (void *)copyFramebufferAddr, i686_TTY_fbInfo.framebufferPitch);
			memcpy((void *)copyFramebufferNewAddr, (void *)copyFramebufferAddr, i686_TTY_fbInfo.framebufferPitch);
		}
	}
	for (size_t screenY = (i686_TTY_YSize - 1) * FONT_fontHeight; screenY < i686_TTY_YSize * FONT_fontHeight;
		 ++screenY) {
		uint32_t copyFramebufferNewAddr = i686_TTY_copyFramebuffer + screenY * i686_TTY_fbInfo.framebufferPitch;
		uint32_t scanlineAddr = i686_TTY_framebuffer + screenY * i686_TTY_fbInfo.framebufferPitch;
		memset((void *)scanlineAddr, 0, i686_TTY_fbInfo.framebufferPitch);
		memset((void *)copyFramebufferNewAddr, 0, i686_TTY_fbInfo.framebufferPitch);
	}
	i686_TTY_X = 0;
	i686_TTY_Y = i686_TTY_YSize - 1;
}

static void i686_TTY_PutCharacterRaw(char c) {
	i686_TTY_PutCharacterAt(i686_TTY_X++, i686_TTY_Y, i686_TTY_Color, c);
	if (i686_TTY_X == i686_TTY_XSize) {
		i686_TTY_X = 0;
		i686_TTY_Y++;
	}
}

static void i686_TTY_PutCharacter(char c) {
	if (i686_TTY_Y == i686_TTY_YSize) {
		i686_TTY_Scroll();
	}
	if (c == '\n') {
		i686_TTY_Y++;
		i686_TTY_X = 0;
	} else if (c == '\t') {
		i686_TTY_X += 1;
		i686_TTY_X += i686_TTY_TabSize - (i686_TTY_X % i686_TTY_TabSize);
		if (i686_TTY_X >= i686_TTY_XSize) {
			i686_TTY_X -= i686_TTY_XSize;
			i686_TTY_Y++;
		}
	} else {
		i686_TTY_PutCharacterRaw(c);
	}
}

void i686_TTY_Initialize() {
	i686_TTY_X = 0;
	i686_TTY_Y = 0;
	if (!i686_Stivale_GetFramebufferInfo(&i686_TTY_fbInfo)) {
		const char msg[] = "[ FAIL ] i686 Kernel Init: Failed to find framebuffer. Please "
						   "make sure that VBE mode is supported, as it is "
						   "required to for CPL-1 kernel to boot";
		i686_TTY_ReportFramebufferError(msg, ARR_SIZE(msg));
	}
	i686_TTY_XSize = i686_TTY_fbInfo.framebufferWidth / FONT_fontWidth;
	i686_TTY_YSize = i686_TTY_fbInfo.framebufferHeight / FONT_fontHeight;
	size_t framebufferSize = i686_TTY_fbInfo.framebufferPitch * i686_TTY_fbInfo.framebufferHeight;
	i686_TTY_copyFramebuffer = (uint32_t)Heap_AllocateMemory(framebufferSize);
	if (i686_TTY_copyFramebuffer == 0) {
		// This will only be printed in e9 log
		KernelLog_ErrorMsg("i686 Terminal", "Failed to create second buffer to use in double buffering");
	}
	uint32_t framebufferPageOffset = (uint32_t)i686_TTY_fbInfo.framebufferAddr -
									 ALIGN_DOWN((uint32_t)i686_TTY_fbInfo.framebufferAddr, HAL_VirtualMM_PageSize);
	uint32_t framebufferPagesStart = ALIGN_DOWN((uint32_t)i686_TTY_fbInfo.framebufferAddr, HAL_VirtualMM_PageSize);
	uint32_t framebufferPagesEnd =
		ALIGN_UP((uint32_t)i686_TTY_fbInfo.framebufferAddr + framebufferSize, HAL_VirtualMM_PageSize);
	uintptr_t framebufferMapping =
		HAL_VirtualMM_GetIOMapping(framebufferPagesStart, framebufferPagesEnd - framebufferPagesStart, false);
	if (framebufferMapping == 0) {
		// This will only be printed in e9 log
		KernelLog_ErrorMsg("i686 Terminal", "Failed to map framebuffer");
	}
	i686_TTY_framebuffer = framebufferMapping + framebufferPageOffset;
	i686_TTY_Color = 0xbbbbbbbb;
	i686_TTY_isFramebufferInitialized = true;
}

void HAL_TTY_Flush() {
}

void HAL_TTY_PrintCharacter(char c) {
	if (i686_TTY_isFramebufferInitialized) {
		i686_TTY_PutCharacter(c);
	}
	i686_Ports_WriteByte(0xe9, c);
}

void HAL_TTY_SetColor(UNUSED uint8_t color) {
}
