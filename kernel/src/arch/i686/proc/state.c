#include <arch/i686/proc/state.h>

size_t HAL_PROCESS_STATE_SIZE = sizeof(struct i686_CPUState);

void HAL_State_EnableInterrupts(void *state) {
    struct i686_CPUState *i686State = (struct i686_CPUState *)state;
    i686State->eflags |= (1 << 9);
}

void HAL_State_DisableInterrupts(void *state) {
    struct i686_CPUState *i686State = (struct i686_CPUState *)state;
    i686State->eflags &= ~(1 << 9);
}

void HAL_State_UpdateIP(void *state, uintptr_t ip) {
    struct i686_CPUState *i686State = (struct i686_CPUState *)state;
    i686State->eip = ip;
}