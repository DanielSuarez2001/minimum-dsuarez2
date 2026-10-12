#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "minemu/syscall.h"
#include "minemu/trace.h"
#include "minemu/platform.h"
#include "minemu/user_abi.h"
#include "minemu/uart.h"
#include "minemu/user_loader.h"

#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR UINT32_C(0x10)
#define CPSR_THUMB UINT32_C(0x20)
#define A32_SVC_ZERO UINT32_C(0xef000000)

static bool translate_user_abi(uint32_t *fd, uint32_t *op) {
    uint32_t request = *op;
    switch (*fd) {
    case MINEMU_FD_UART_IN:
        *fd = MINEMU_UART0_BASE;
        if (request == MINEMU_IOCTL_UART_READ) *op = 0;
        else return false;
        return true;
    case MINEMU_FD_UART_OUT:
        *fd = MINEMU_UART0_BASE;
        if (request == MINEMU_IOCTL_UART_WRITE) *op = 1;
        else return false;
        return true;
    case MINEMU_FD_UART_DIAG:
        *fd = MINEMU_UART1_BASE;
        if (request == MINEMU_IOCTL_UART_READ) *op = 0;
        else if (request == MINEMU_IOCTL_UART_WRITE) *op = 1;
        else return false;
        return true;
    case MINEMU_FD_RNG:
        *fd = MINEMU_RNG_BASE;
        if (request == MINEMU_IOCTL_RNG_SEED) *op = 0;
        else if (request == MINEMU_IOCTL_RNG_NEXT) *op = 1;
        else return false;
        return true;
    case MINEMU_FD_TRACE:
        *fd = MINEMU_TRACE_BASE;
        if (request == MINEMU_IOCTL_TRACE_EVENT) *op = 0;
        else return false;
        return true;
    default:
        return true;
    }
}

struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame) {
    uint32_t mode = frame->spsr & CPSR_MODE_MASK;
    if (frame->exception_id != MINEMU_EXCEPTION_SVC || mode != CPSR_MODE_USR ||
        (frame->spsr & CPSR_THUMB) != 0) {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }

    uint32_t instruction = *(const uint32_t *)(uintptr_t)frame->fault_pc;
    if (instruction != A32_SVC_ZERO || frame->r[7] != MINEMU_SYSCALL_IOCTL) {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }

    if (frame->r[7] == MINEMU_SYSCALL_IOCTL) {
        uint32_t fd = frame->r[0];
        uint32_t op = frame->r[1];
        uint32_t arg0 = frame->r[2];
        uint32_t arg1 = frame->r[3];

        //Special handling for RNG state request
        if (fd == MINEMU_FD_RNG && op == MINEMU_IOCTL_RNG_STATE) {
            if (user_range_valid(arg0, sizeof(uint32_t), true)) {
                *(uint32_t *)(uintptr_t)arg0 = MINEMU_RNG->state;
                frame->r[0] = 0;
                return frame;
            } else {
                frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
                return frame;
            }
        }
        //Translate user ABI to internal representation
        if (!translate_user_abi(&fd, &op)) {
            frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
            return frame;
        }
        //File descriptor for RNG
        if (fd == MINEMU_RNG_BASE) {
            //Set the seed for RNG
            if (op == UINT32_C(0x0)) {
                MINEMU_RNG->seed = arg0;
                frame->r[0] = 0;
                return frame;
            }
            //Get data from RNG
            else if (op == UINT32_C(0x1) && 
            user_range_valid(arg0, sizeof(uint32_t), true)) {
                *(uint32_t *)(uintptr_t)arg0 = MINEMU_RNG->data;
                frame->r[0] = 0;
                return frame;
            }
            //Unknown operation for RNG, fallback to error
            else {
                frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
                return frame;
            }
        }
        //File descriptor for UART0
        else if (fd == MINEMU_UART0_BASE) {
            //Read from UART0 RX
            if (op == UINT32_C(0x0) && 
            !(arg1 > MINEMU_IOCTL_MAX_TRANSFER) &&
            user_range_valid(arg0, (uint32_t)arg1, true)) {
                char *dst = (char *)(uintptr_t)arg0;
                size_t n = (size_t)arg1;
                frame->r[0] = (uint32_t)(uart0_var_getchars(dst, n));
                return frame;
            }
            //Write to UART0 TX
            else if (op == UINT32_C(0x1) && 
            !(arg1 > MINEMU_IOCTL_MAX_TRANSFER) &&
            user_range_valid(arg0, (uint32_t)arg1, false)) {
                const char *src = (const char *)(uintptr_t)arg0;
                size_t n = (size_t)arg1;
                frame->r[0] = (uint32_t)(uart0_var_putstring(src, n));
                return frame;
            }
            //Unknown operation for UART0, fallback to error
            else {
                frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
                return frame;
            }
        }
        //File descriptor for UART1
        else if (fd == MINEMU_UART1_BASE) {
            //Read from UART1 RX
            if (op == UINT32_C(0x0) && 
            !(arg1 > MINEMU_IOCTL_MAX_TRANSFER) &&
            user_range_valid(arg0, (uint32_t)arg1, true)) {
                char *dst = (char *)(uintptr_t)arg0;
                size_t n = (size_t)arg1;
                frame->r[0] = (uint32_t)(uart1_var_getchars(dst, n));
                return frame;
            }
            //Write to UART1 TX
            else if (op == UINT32_C(0x1) &&
            !(arg1 > MINEMU_IOCTL_MAX_TRANSFER) &&
            user_range_valid(arg0, (uint32_t)arg1, false)) {
                const char *src = (const char *)(uintptr_t)arg0;
                size_t n = (size_t)arg1;
                frame->r[0] = (uint32_t)(uart1_var_putstring(src, n));
                return frame;
            }
            //Unknown operation for UART1, fallback to error
            else {
                frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
                return frame;
            }
        }
        //File descriptor for TRACE
        else if (fd == MINEMU_TRACE_BASE) {
            //Generate trace event
            if (op == UINT32_C(0x0)) {
                minemu_trace_event((uint32_t)(uintptr_t)arg0);
                frame->r[0] = 0;
                return frame;
            }
            //Unknown operation for TRACE, fallback to error
            else {
                frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
                return frame;
            }
        }
        //Unknown file descriptor, fallback to error
        else {
            frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
            return frame;
        }
    }
    return frame;
}