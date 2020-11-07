#ifndef __PRIV_H_INCLUDED__
#define __PRIV_H_INCLUDED__

void priv_init();
void priv_call_ring0(uint32_t function, uint32_t argument);

#endif