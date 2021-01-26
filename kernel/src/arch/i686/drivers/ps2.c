#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/ps2.h>
#include <common/lib/kmsg.h>
#include <hal/proc/intlevel.h>

#define i686_PS2_DATA_PORT 0x60
#define i686_PS2_CMD_PORT 0x64

static uint8_t i686_PS2_ReadData() {
	return i686_Ports_ReadByte(i686_PS2_DATA_PORT);
}

static void i686_PS2_WriteData(uint8_t data) {
	return i686_Ports_WriteByte(i686_PS2_DATA_PORT, data);
}

static uint8_t i686_PS2_ReadStatus() {
	return i686_Ports_ReadByte(i686_PS2_CMD_PORT);
}

static void i686_PS2_WriteCommand(uint8_t cmd) {
	return i686_Ports_WriteByte(i686_PS2_CMD_PORT, cmd);
}

static void i686_PS2_AwaitResult() {
	uint8_t status = i686_PS2_ReadStatus();
	while ((status & (1 << 0)) == 0) {
		status = i686_PS2_ReadStatus();
	}
}

static uint8_t i686_PS2_ExecuteByteCommand(uint8_t cmd, bool response) {
	i686_PS2_WriteCommand(cmd);
	if (response) {
		i686_PS2_AwaitResult();
		return i686_PS2_ReadData();
	}
	return 0;
}

static uint8_t i686_PS2_ExecuteWordCommand(uint8_t cmd1, uint8_t cmd2, bool response) {
	i686_PS2_WriteCommand(cmd1);
	uint8_t status = i686_PS2_ReadStatus();
	while ((status & (1 << 1)) != 0) {
		status = i686_PS2_ReadStatus();
	}
	i686_PS2_WriteData(cmd2);
	if (response) {
		i686_PS2_AwaitResult();
		return i686_PS2_ReadData();
	}
	return 0;
}

#define i686_PS2_ATTEMPTS_COUNT 65536
#define i686_PS2_ACK 0xFA
#define i686_PS2_RESEND 0xFE
#define i686_PS2_DISABLE_SCANNING_CMD 0xF5
#define i686_PS2_IDENTIFY_CMD 0xF2

bool i686_PS2_SendByteAndWaitForAck(bool channel, uint8_t byte) {
	while (true) {
		// Try to send command
		if (!i686_PS2_SendByte(channel, byte)) {
			return false;
		}
		// Wait for result
		i686_PS2_AwaitResult();
		uint8_t data = i686_PS2_ReadData();
		if (data == i686_PS2_ACK) {
			// Command completed, return
			return true;
		} else if (data == i686_PS2_RESEND) {
			// Device wants us to resend command. Ok
			continue;
		}
		// Device sent something wrong, return
		return false;
	}
}

// Scanning left disabled for device driver to enable it
static enum i686_PS2_DeviceType i686_PS2_GetDeviceType(bool channel) {
	// Disable scanning
	if (!i686_PS2_SendByteAndWaitForAck(channel, i686_PS2_DISABLE_SCANNING_CMD)) {
		KernelLog_WarnMsg("PS/2 Controller Driver",
						  "i686_PS2_GetDeviceType: result for disable scanning command is not ACK");
		return 0;
	}
	// Identify
	if (!i686_PS2_SendByteAndWaitForAck(channel, i686_PS2_IDENTIFY_CMD)) {
		KernelLog_WarnMsg("PS/2 Controller Driver", "i686_PS2_GetDeviceType: result for identify command is not ACK");
		return 0;
	}
	// Loop waiting for incoming data
	int count = 0;
	uint8_t buf[2];
	for (int i = 0; i < i686_PS2_ATTEMPTS_COUNT; ++i) {
		uint8_t status = i686_PS2_ReadStatus();
		if ((status & (1 << 0)) != 0) {
			buf[count++] = i686_PS2_ReadData();
			if (count == 2) {
				break;
			}
		}
	}
	// Parse device type
	enum i686_PS2_DeviceType type = Unknown;
	if (count == 0) {
		type = Keyboard;
	} else if (count == 1) {
		if (buf[0] == 0) {
			type = Mouse;
		} else if (buf[0] == 0x03) {
			type = Mouse;
		} else if (buf[0] == 0x04) {
			type = Mouse;
		}
	} else {
		if (buf[0] == 0xab && buf[1] == 0x41) {
			type = Keyboard;
		} else if (buf[0] == 0xab && buf[1] == 0xc1) {
			type = Keyboard;
		} else if (buf[0] == 0xab && buf[1] == 0x83) {
			type = Keyboard;
		}
	}
	return type;
}

// https://wiki.osdev.org/%228042%22_PS/2_Controller
void i686_PS2_Enumerate(void (*callback)(bool channel, enum i686_PS2_DeviceType type, void *ctx), void *ctx) {
	// Disable devices
	i686_PS2_ExecuteByteCommand(0xad, false);
	KernelLog_InfoMsg("PS/2 Controller Driver", "Devices disabled");
	// Flush buffers by reading data
	i686_PS2_ReadData();
	KernelLog_InfoMsg("PS/2 Controller Driver", "Data flushed");
	// Disabling ints and translation
	uint8_t config = i686_PS2_ExecuteByteCommand(0x20, true);
	config &= ~((1 << 0) | (1 << 1) | (1 << 6));
	bool single = (config & (1 << 5)) != 0;
	i686_PS2_ExecuteWordCommand(0x60, config, false);
	KernelLog_InfoMsg("PS/2 Controller Driver", "Interrupts and translation disabled");
	// Self-test
	uint8_t result = i686_PS2_ExecuteByteCommand(0xaa, true);
	if (result != 0x55) {
		KernelLog_ErrorMsg("PS/2 Controller Driver", "Failed to self-test PS/2 controller");
	}
	KernelLog_InfoMsg("PS/2 Controller Driver", "Self-test passed");
	// Testing first port
	if (i686_PS2_ExecuteByteCommand(0xab, true) == 0x00) {
		KernelLog_InfoMsg("PS/2 Controller Driver", "Enabling first device");
		// Enable device
		i686_PS2_ExecuteByteCommand(0xae, false);
		KernelLog_InfoMsg("PS/2 Controller Driver", "Enable command done");
		// Reset
		if (!i686_PS2_SendByte(false, 0xff)) {
			KernelLog_WarnMsg("PS/2 Controller Driver", "Reset operation timed out. Skipping");
		} else {
			KernelLog_InfoMsg("PS/2 Controller Driver", "Reset command done");
			// Get device type
			enum i686_PS2_DeviceType type = i686_PS2_GetDeviceType(false);
			if (type != Unknown) {
				callback(false, type, ctx);
			}
		}
	} else {
		KernelLog_InfoMsg("PS/2 Controller Driver", "Failed to test first PS/2 controller channel");
	}
	// Testing second port
	if (!single) {
		if (i686_PS2_ExecuteByteCommand(0xa9, true) == 0x00) {
			KernelLog_InfoMsg("PS/2 Controller Driver", "Enabling second device");
			// Enable device
			i686_PS2_ExecuteByteCommand(0xa8, false);
			KernelLog_InfoMsg("PS/2 Controller Driver", "Enable command done");
			// Reset
			if (!i686_PS2_SendByte(true, 0xff)) {
				KernelLog_WarnMsg("PS/2 Controller Driver", "Reset operation timed out. Skipping");
			} else {
				KernelLog_InfoMsg("PS/2 Controller Driver", "Reset command done");
				// Get device type
				enum i686_PS2_DeviceType type = i686_PS2_GetDeviceType(true);
				if (type != Unknown) {
					callback(false, type, ctx);
				}
			}
		} else {
			KernelLog_InfoMsg("PS/2 Controller Driver", "Failed to test second PS/2 controller channel");
		}
	}
	// Enable IRQs
	KernelLog_InfoMsg("PS/2 Controller Driver", "Enabling IRQs");
	config = i686_PS2_ExecuteByteCommand(0x20, true);
	config |= ((1 << 0) | (1 << 1));
	i686_PS2_ExecuteWordCommand(0x60, config, false);
}

bool i686_PS2_SendByte(bool channel, uint8_t byte) {
	if (channel) {
		i686_PS2_WriteCommand(0xD4);
	}
	for (int i = 0; i < i686_PS2_ATTEMPTS_COUNT; ++i) {
		uint8_t status = i686_PS2_ReadStatus();
		if ((status & (1 << 1)) == 0) {
			goto send;
		}
	}
	return false;
send:
	i686_PS2_WriteData(byte);
	return true;
}