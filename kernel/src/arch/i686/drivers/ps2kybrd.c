#include <arch/i686/drivers/ps2.h>
#include <arch/i686/drivers/ps2kybrd.h>
#include <arch/i686/proc/iowait.h>
#include <common/lib/kmsg.h>
#include <common/misc/utils.h>
#include <hal/drivers/tty.h>

////////////////////////////////// THIRD PARTY CODE //////////////////////////////////
// Copyright (c) 2018-2020, the qword authors (AUTHORS.md)
// All rights reserved.

// Redistribution and use in source form, or binary form with accompanying source,
// with or without modification, are permitted provided that the following conditions
// are met:
//    * The redistribution of the source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Modified versions of the source code MUST be licensed under a verbatim
//      copy of the present License Agreement.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#define PS2_KYBRD_MAX_CODE 0x57
#define PS2_KYBRD_CAPSLOCK 0x3a
#define PS2_KYBRD_LEFT_ALT 0x38
#define PS2_KYBRD_LEFT_ALT_REL 0xb8
#define PS2_KYBRD_RIGHT_SHIFT 0x36
#define PS2_KYBRD_LEFT_SHIFT 0x2a
#define PS2_KYBRD_RIGHT_SHIFT_REL 0xb6
#define PS2_KYBRD_LEFT_SHIFT_REL 0xaa
#define PS2_KYBRD_CTRL 0x1d
#define PS2_KYBRD_CTRL_REL 0x9d

static const char m_asciiCapslockTable[] = {
	'\0', '\e', '1',  '2',	'3', '4', '5', '6', '7',  '8',	'9', '0', '-', '=', '\b', '\t', 'Q',  'W', 'E', 'R',
	'T',  'Y',	'U',  'I',	'O', 'P', '[', ']', '\n', '\0', 'A', 'S', 'D', 'F', 'G',  'H',	'J',  'K', 'L', ';',
	'\'', '`',	'\0', '\\', 'Z', 'X', 'C', 'V', 'B',  'N',	'M', ',', '.', '/', '\0', '\0', '\0', ' '};

static const char m_asciiShiftTable[] = {
	'\0', '\e', '!',  '@', '#', '$', '%', '^', '&',	 '*',  '(', ')', '_', '+', '\b', '\t', 'Q',	 'W', 'E', 'R',
	'T',  'Y',	'U',  'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S', 'D', 'F', 'G',	 'H',  'J',	 'K', 'L', ':',
	'"',  '~',	'\0', '|', 'Z', 'X', 'C', 'V', 'B',	 'N',  'M', '<', '>', '?', '\0', '\0', '\0', ' '};

static const char m_asciiShiftCapslockTable[] = {
	'\0', '\e', '!',  '@', '#', '$', '%', '^', '&',	 '*',  '(', ')', '_', '+', '\b', '\t', 'q',	 'w', 'e', 'r',
	't',  'y',	'u',  'i', 'o', 'p', '{', '}', '\n', '\0', 'a', 's', 'd', 'f', 'g',	 'h',  'j',	 'k', 'l', ':',
	'"',  '~',	'\0', '|', 'z', 'x', 'c', 'v', 'b',	 'n',  'm', '<', '>', '?', '\0', '\0', '\0', ' '};

static const char m_asciiBase[] = {'\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8',	'9',  '0',	'-',  '=',	'\b',
								   '\t', 'q',  'w', 'e', 'r', 't', 'y', 'u', 'i', 'o',	'p',  '[',	']',  '\n', '\0',
								   'a',	 's',  'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	'\'', '`',	'\0', '\\', 'z',
								   'x',	 'c',  'v', 'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '};
////////////////////////////////// THIRD PARTY CODE END //////////////////////////////////

static bool m_isKeyboardInitialized = false;
static bool m_ps2KeyboardChannel;
static bool m_capslockPressed;
static bool m_leftShiftPressed;
static bool m_rightShiftPressed;
static bool m_shiftPressed;
static struct HAL_TTY_KeyEvent m_event;
static struct i686_IOWait_ListEntry *m_iowaitObject;

void i686_PS2Keyboard_IRQCallback(MAYBE_UNUSED void *ctx, MAYBE_UNUSED char *state) {
	uint8_t code = i686_PS2_ReadData();
	m_event.raw = code;
	m_event.typeable = false;
	m_event.pressed = (code & (1 << 7)) == 0;
	uint8_t key = code & ~(1 << 7);
	// handle special keys
	switch (key) {
	case PS2_KYBRD_CAPSLOCK:
		if (!(m_event.pressed)) {
			m_capslockPressed = !m_capslockPressed;
		}
		return;
	case PS2_KYBRD_LEFT_ALT:
	case PS2_KYBRD_LEFT_ALT_REL:
	case PS2_KYBRD_LEFT_SHIFT_REL:
	case PS2_KYBRD_RIGHT_SHIFT_REL:
	case PS2_KYBRD_CTRL:
	case PS2_KYBRD_CTRL_REL:
		return;
	case PS2_KYBRD_RIGHT_SHIFT:
		m_rightShiftPressed = !m_rightShiftPressed;
		m_shiftPressed = m_leftShiftPressed || m_rightShiftPressed;
		return;
	case PS2_KYBRD_LEFT_SHIFT:
		m_leftShiftPressed = !m_leftShiftPressed;
		m_shiftPressed = m_leftShiftPressed || m_rightShiftPressed;
		return;
	default:
		break;
	}
	// check if key is typable
	if (key > PS2_KYBRD_MAX_CODE) {
		return;
	}
	// select key map
	const char *keymap = m_asciiBase;
	if (m_shiftPressed && m_capslockPressed) {
		keymap = m_asciiShiftCapslockTable;
	} else if (m_shiftPressed) {
		keymap = m_asciiShiftTable;
	} else if (m_capslockPressed) {
		keymap = m_asciiCapslockTable;
	}
	// If key is not present, return
	if (keymap[key] == '\0') {
		return;
	}
	m_event.typeable = true;
	m_event.character = keymap[key];
}

bool i686_PS2Keyboard_Detect(bool channel) {
	if (m_isKeyboardInitialized) {
		// If keyboard is already attached, ignore a new one
		KernelLog_WarnMsg("PS/2 Keyboard Driver", "Ignoring another PS/2 keyboard");
		return false;
	}
	m_ps2KeyboardChannel = channel;
	// Set scancode set to 1
	if (!(i686_PS2_SendByteAndWaitForAck(channel, 0xF0) && i686_PS2_SendByteAndWaitForAck(channel, 1))) {
		KernelLog_WarnMsg("PS/2 Keyboard Driver", "Failed to set scancode set to set 1");
		return false;
	}
	KernelLog_InfoMsg("PS/2 Keyboard Driver", "Set scancode set to 2");
	// Turn off all LEDs
	if (!(i686_PS2_SendByteAndWaitForAck(channel, 0xED) && i686_PS2_SendByteAndWaitForAck(channel, 0))) {
		// This error is not critical, so it can be skipped
		KernelLog_WarnMsg("PS/2 Keyboard Driver", "Failed to turn off LEDs");
	}
	KernelLog_WarnMsg("PS/2 Keyboard Driver", "LEDs turned off. LED updates are not supported yet");
	// Okey, keyboard is ready
	m_capslockPressed = false;
	m_leftShiftPressed = false;
	m_rightShiftPressed = false;
	m_shiftPressed = false;
	// Load interrupt handler
	m_iowaitObject = i686_IOWait_AddHandler(1, (i686_IOWait_Handler)i686_PS2Keyboard_IRQCallback, NULL, NULL);
	if (m_iowaitObject == NULL) {
		KernelLog_WarnMsg("PS/2 Keyboard Driver", "Failed to load keyboard interrupt handler");
		return false;
	}
	// Enable scanning
	i686_PS2_SendByteAndWaitForAck(channel, 0xf4);
	KernelLog_InfoMsg("PS/2 Keyboard Driver", "Keyboard setup done");
	return true;
}

void HAL_TTY_WaitForNextEvent(struct HAL_TTY_KeyEvent *event) {
	i686_IOWait_WaitForIRQ(m_iowaitObject);
	*event = m_event;
}