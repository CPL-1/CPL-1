#ifndef __PRIV_H_INCLUDED__
#define __PRIV_H_INCLUDED__

void i686_Ring0Executor_Initialize();
void i686_Ring0Executor_Invoke(uint32_t function, uint32_t argument);

#endif