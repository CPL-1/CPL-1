#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/tty.h>
#include <arch/i686/init/stivale.h>
#include <common/core/memory/heap.h>
#include <common/core/memory/iomap.h>
#include <common/lib/kmsg.h>
#include <common/misc/font.h>
#include <hal/drivers/tty.h>
#include <hal/memory/virt.h>

static uint16_t m_ttyY, m_ttyX;
static uint16_t m_ttyXSize, m_ttyYSize;
static uint32_t m_framebuffer, m_backbuffer;

static struct i686_Stivale_FramebufferInfo m_fbInfo;

static bool m_isFramebufferInitialized = false;

static uint32_t m_ttyForegroundColor;
static uint32_t m_ttyBackgroundColor;

static uint32_t m_ttyDefaultForegroundColor;
static uint32_t m_ttyDefaultBackgroundColor;

static uint32_t m_ttyTabSize = 4;

static uint32_t m_VGA2RGB[] = {0x00000000, 0x00000080, 0x00008000, 0x00008080, 0x00800000, 0x00800080,
							   0x00808000, 0x00c0c0c0, 0x00808080, 0x000000ff, 0x0000ff00, 0x0000ffff,
							   0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff};

// Array with boolean for each line
// true if break is allowed to move to the previous line
static bool *m_allowBreakage;

static void i686_TTY_ReportFramebufferError(const char *msg, size_t size) {
	memset((void *)(HAL_VirtualMM_KernelMappingBase + 0xb8000), 0, 4000);
	for (size_t i = 0; i < size; ++i) {
		((char *)(HAL_VirtualMM_KernelMappingBase + 0xb8000))[2 * i] = msg[i];
		((char *)(HAL_VirtualMM_KernelMappingBase + 0xb8000))[2 * i + 1] = 0x0f;
	}
	while (true) {
		ASM VOLATILE("nop");
	}
};

static void i686_TTY_PutCharacterAt(uint16_t x, uint16_t y, char c) {
	c -= FONT_FirstPrintableChar;
	for (uint16_t charY = 0; charY < FONT_FontHeight; ++charY) {
		for (uint16_t charX = 0; charX < FONT_FontWidth; ++charX) {
			size_t bitmapOffset = c * FONT_FontHeight * FONT_FontWidth / 8;
			size_t bitOffset = charY * FONT_FontWidth + charX;
			size_t byteOffset = bitOffset / 8;
			size_t bitOffsetModule = bitOffset % 8;
			bool drawPixel = (FONT_bitmap[bitmapOffset + byteOffset] & (1 << (FONT_FontWidth - bitOffsetModule))) != 0;
			uint16_t screenX = x * FONT_FontWidth + charX;
			uint16_t screenY = y * FONT_FontHeight + charY;
			uint32_t framebufferAddr = m_framebuffer + screenY * m_fbInfo.framebufferPitch + screenX * 4;
			uint32_t copyFramebufferAddr = m_backbuffer + screenY * m_fbInfo.framebufferPitch + screenX * 4;

			if (drawPixel) {
				*(uint32_t *)framebufferAddr = *(uint32_t *)copyFramebufferAddr = m_ttyForegroundColor;
			} else {
				*(uint32_t *)framebufferAddr = *(uint32_t *)copyFramebufferAddr = m_ttyBackgroundColor;
			}
		}
	}
}

static void i686_TTY_Scroll(bool allowBreak) {
	for (size_t y = 0; y < m_ttyYSize - 1U; ++y) {
		for (size_t screenY = y * FONT_FontHeight; screenY < (y + 1) * FONT_FontHeight; ++screenY) {
			uint32_t scanlineAddr = m_framebuffer + screenY * m_fbInfo.framebufferPitch;
			uint32_t copyFramebufferNewAddr = m_backbuffer + screenY * m_fbInfo.framebufferPitch;
			uint32_t copyFramebufferAddr = m_backbuffer + (screenY + FONT_FontHeight) * m_fbInfo.framebufferPitch;
			memcpy((void *)scanlineAddr, (void *)copyFramebufferAddr, m_fbInfo.framebufferPitch);
			memcpy((void *)copyFramebufferNewAddr, (void *)copyFramebufferAddr, m_fbInfo.framebufferPitch);
		}
	}
	for (size_t screenY = (m_ttyYSize - 1) * FONT_FontHeight; screenY < m_ttyYSize * FONT_FontHeight; ++screenY) {
		uint32_t copyFramebufferNewAddr = m_backbuffer + screenY * m_fbInfo.framebufferPitch;
		uint32_t scanlineAddr = m_framebuffer + screenY * m_fbInfo.framebufferPitch;
		for (size_t x = 0; x < m_fbInfo.framebufferWidth; ++x) {
			((uint32_t *)scanlineAddr)[x] = m_ttyDefaultBackgroundColor;
			((uint32_t *)copyFramebufferNewAddr)[x] = m_ttyDefaultBackgroundColor;
		}
	}
	m_ttyX = 0;
	m_ttyY = m_ttyYSize - 1;
	for (int y = 1; y < m_ttyYSize - 1; ++y) {
		m_allowBreakage[y] = m_allowBreakage[y + 1];
	}
	m_allowBreakage[m_ttyYSize - 1] = allowBreak;
	m_allowBreakage[0] = false;
}

static void i686_TTY_PutCharacterRaw(char c) {
	i686_TTY_PutCharacterAt(m_ttyX++, m_ttyY, c);
	if (m_ttyX == m_ttyXSize) {
		m_ttyX = 0;
		m_ttyY++;
		if (m_ttyY < m_ttyYSize) {
			m_allowBreakage[m_ttyY] = true;
		}
	}
}

static void i686_TTY_PutCharacter(char c) {
	if (m_ttyY == m_ttyYSize) {
		i686_TTY_Scroll(true);
	}
	if (c == '\n') {
		uint16_t oldY = m_ttyY;
		while (oldY == m_ttyY) {
			i686_TTY_PutCharacterRaw(' ');
		}
		if (m_ttyY == m_ttyYSize) {
			i686_TTY_Scroll(false);
		} else {
			m_allowBreakage[m_ttyY] = false;
		}
	} else if (c == '\t') {
		uint16_t newX = m_ttyX;
		newX += 1;
		newX += m_ttyTabSize - (newX % m_ttyTabSize);
		if (newX >= m_ttyXSize) {
			newX -= m_ttyXSize;
		}
		while (m_ttyX != newX) {
			i686_TTY_PutCharacterRaw(' ');
		}
	} else if (c == '\b') {
		if (m_ttyX == 0) {
			if (m_allowBreakage[m_ttyY]) {
				if (m_ttyY == 0) {
					KernelLog_ErrorMsg("i686 Terminal", "Breakage out of line 0 is allowed");
				}
				m_ttyY--;
				m_ttyX = m_ttyXSize - 1;
			}
		} else {
			m_ttyX--;
		}
	} else {
		i686_TTY_PutCharacterRaw(c);
	}
}

void i686_TTY_Initialize() {
	if (!i686_Stivale_GetFramebufferInfo(&m_fbInfo)) {
		const char msg[] = "[ FAIL ] i686 Kernel Init: Failed to find framebuffer. Please "
						   "make sure that VBE mode is supported, as it is "
						   "required to for CPL-1 kernel to boot";
		i686_TTY_ReportFramebufferError(msg, ARR_SIZE(msg));
	}
	m_ttyXSize = m_fbInfo.framebufferWidth / FONT_FontWidth;
	m_ttyYSize = m_fbInfo.framebufferHeight / FONT_FontHeight;
	size_t framebufferSize = m_fbInfo.framebufferPitch * m_fbInfo.framebufferHeight;
	m_backbuffer = (uint32_t)Heap_AllocateMemory(framebufferSize);
	if (m_backbuffer == 0) {
		// This will only be printed in e9 log
		KernelLog_ErrorMsg("i686 Terminal", "Failed to create second buffer to use in double buffering");
	}
	uint32_t framebufferPageOffset =
		(uint32_t)m_fbInfo.framebufferAddr - ALIGN_DOWN((uint32_t)m_fbInfo.framebufferAddr, HAL_VirtualMM_PageSize);
	uint32_t framebufferPagesStart = ALIGN_DOWN((uint32_t)m_fbInfo.framebufferAddr, HAL_VirtualMM_PageSize);
	uint32_t framebufferPagesEnd =
		ALIGN_UP((uint32_t)m_fbInfo.framebufferAddr + framebufferSize, HAL_VirtualMM_PageSize);
	uintptr_t framebufferMapping =
		IOMap_AllocateIOMapping(framebufferPagesStart, framebufferPagesEnd - framebufferPagesStart, false);
	if (framebufferMapping == 0) {
		// This will only be printed in e9 log
		KernelLog_ErrorMsg("i686 Terminal", "Failed to map framebuffer");
	}
	m_allowBreakage = Heap_AllocateMemory(sizeof(bool) * m_ttyXSize);
	if (m_allowBreakage == NULL) {
		// This will only be printed in e9 log
		KernelLog_ErrorMsg("i686 Terminal", "Failed to map framebuffer");
	}
	for (int i = 0; i < m_ttyXSize; ++i) {
		m_allowBreakage[i] = false;
	}
	m_framebuffer = framebufferMapping + framebufferPageOffset;
	m_ttyForegroundColor = m_VGA2RGB[7];
	m_ttyBackgroundColor = /*0x002c001e*/ 0;
	m_ttyDefaultForegroundColor = m_ttyForegroundColor;
	m_ttyDefaultBackgroundColor = m_ttyBackgroundColor;
	m_isFramebufferInitialized = true;
	HAL_TTY_Clear();
}

void HAL_TTY_Flush() {
}

void HAL_TTY_PrintCharacter(char c) {
	if (m_isFramebufferInitialized) {
		i686_TTY_PutCharacter(c);
	}
	i686_Ports_WriteByte(0xe9, c);
}

void HAL_TTY_SetForegroundColor(uint8_t color) {
	if (color > 17) {
		return;
	}
	if (color == 17) {
		m_ttyForegroundColor = m_ttyDefaultForegroundColor;
	} else {
		m_ttyForegroundColor = m_VGA2RGB[color];
	}
}

void HAL_TTY_SetBackgroundColor(uint8_t color) {
	if (color > 17) {
		return;
	}
	if (color == 17) {
		m_ttyBackgroundColor = m_ttyDefaultBackgroundColor;
	} else {
		m_ttyBackgroundColor = m_VGA2RGB[color];
	}
}

void HAL_TTY_Clear() {
	for (size_t y = 0; y < m_fbInfo.framebufferHeight; ++y) {
		for (size_t x = 0; x < m_fbInfo.framebufferWidth; ++x) {
			uint32_t addr = (uint32_t)m_framebuffer + (y * m_fbInfo.framebufferPitch) + 4 * x;
			*(uint32_t *)addr = m_ttyDefaultBackgroundColor;
		}
	}
	m_ttyX = 0;
	m_ttyY = 0;
}
