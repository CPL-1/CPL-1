#ifndef __PRIV_H_INCLUDED__
#define __PRIV_H_INCLUDED__

void i686_Ring0Executor_Initialize();
void i686_Ring0Executor_Invoke(UINT32 function, UINT32 argument);

#endif