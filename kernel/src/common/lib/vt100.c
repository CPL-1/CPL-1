#include <common/lib/vt100.h>
#include <common/misc/utils.h>
#include <hal/drivers/tty.h>

static enum
{
	VT100_Raw,
	VT100_EscapeEncountered,
	VT100_LeftParenEncountered,
	VT100_EnteringForegroundColor,
	VT100_EnteringBackgroundColor,
	VT100_EnteringBackgroundShiftedColor,
	VT100_AwaitingForBackgroundColor,
	VT100_AwaitingForTerm,
	VT100_AwaitingForTermOrBackgroundColor,
} m_state = VT100_Raw;

static bool m_modifyBackgroundColor;
static bool m_modifyForegroundColor;
static bool m_shiftBackgroundColor;
static bool m_shiftForegroundColor;
static uint8_t m_requestedBackgroundColor;
static uint8_t m_requestedForegroundColor;

static void VT100_ExecuteColorChangeRequest() {
	if (m_modifyForegroundColor) {
		uint8_t delta = (m_shiftForegroundColor && m_requestedForegroundColor != 17) ? 8 : 0;
		HAL_TTY_SetForegroundColor(m_requestedForegroundColor + delta);
	}
	if (m_modifyBackgroundColor) {
		uint8_t delta = (m_shiftBackgroundColor && m_requestedBackgroundColor != 17) ? 8 : 0;
		HAL_TTY_SetBackgroundColor(m_requestedBackgroundColor + delta);
	}
}

void VT100_PutCharacter(char c) {
	switch (m_state) {
	case VT100_Raw: {
		if (c == '\033') {
			m_state = VT100_EscapeEncountered;
			m_modifyBackgroundColor = false;
			m_modifyForegroundColor = false;
		} else {
			HAL_TTY_PrintCharacter(c);
		}
	} break;
	case VT100_EscapeEncountered: {
		if (c == '[') {
			m_state = VT100_LeftParenEncountered;
		} else {
			m_state = VT100_Raw;
		}
	} break;
	case VT100_LeftParenEncountered: {
		if (c == '0') {
			m_state = VT100_AwaitingForTerm;
			m_modifyBackgroundColor = true;
			m_modifyForegroundColor = true;
			m_requestedBackgroundColor = 9;
			m_requestedForegroundColor = 9;
		} else if (c == '3') {
			m_state = VT100_EnteringForegroundColor;
			m_shiftForegroundColor = false;
		} else if (c == '4') {
			m_state = VT100_EnteringBackgroundColor;
			m_shiftBackgroundColor = false;
		} else if (c == '9') {
			m_state = VT100_EnteringForegroundColor;
			m_shiftForegroundColor = true;
		} else if (c == '1') {
			m_state = VT100_EnteringBackgroundShiftedColor;
			m_shiftForegroundColor = true;
		} else {
			m_state = VT100_Raw;
		}
	} break;
	case VT100_EnteringForegroundColor: {
		if (c >= '0' && c <= '9' && c != '8') {
			uint8_t color = c - '0';
			if (color == 9) {
				color = 17;
			}
			m_modifyForegroundColor = true;
			m_requestedForegroundColor = color;
			m_state = VT100_AwaitingForTermOrBackgroundColor;
		} else {
			m_state = VT100_Raw;
		}
	} break;
	case VT100_EnteringBackgroundColor: {
		if (c >= '0' && c <= '9' && c != '8') {
			uint8_t color = c - '0';
			m_modifyBackgroundColor = true;
			if (color == 9) {
				color = 17;
			}
			m_requestedBackgroundColor = color;
			m_state = VT100_AwaitingForTerm;
		} else {
			m_state = VT100_Raw;
		}
	} break;
	case VT100_EnteringBackgroundShiftedColor: {
		if (c == '0') {
			m_state = VT100_EnteringBackgroundColor;
		} else {
			m_state = VT100_Raw;
		}
	} break;
	case VT100_AwaitingForTermOrBackgroundColor: {
		if (c == ';') {
			m_state = VT100_AwaitingForBackgroundColor;
		} else if (c == 'm') {
			VT100_ExecuteColorChangeRequest();
			m_state = VT100_Raw;
		}
	} break;
	case VT100_AwaitingForBackgroundColor: {
		if (c == '4') {
			m_state = VT100_EnteringBackgroundColor;
			m_shiftBackgroundColor = false;
		} else if (c == '1') {
			m_state = VT100_EnteringBackgroundShiftedColor;
			m_shiftBackgroundColor = true;
		} else {
			m_state = VT100_Raw;
		}
	} break;
	case VT100_AwaitingForTerm: {
		if (c == 'm') {
			VT100_ExecuteColorChangeRequest();
			m_state = VT100_Raw;
		} else {
			m_state = VT100_Raw;
		}
	}
	}
}

void VT100_Flush() {
	HAL_TTY_Flush();
}