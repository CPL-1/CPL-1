#ifndef __I686_PS2_H_INCLUDED__
#define __I686_PS2_H_INCLUDED__

#include <common/misc/utils.h>

enum i686_PS2_DeviceType { Unknown, Keyboard, Mouse };

uint8_t i686_PS2_ReadData();
bool i686_PS2_ReadyToRead();
void i686_PS2_AwaitResult();
bool i686_PS2_SendByte(bool channel, uint8_t byte);
bool i686_PS2_SendByteAndWaitForAck(bool channel, uint8_t byte);
void i686_PS2_Enumerate(void (*callback)(bool channel, enum i686_PS2_DeviceType type, void *ctx), void *ctx);

#endif