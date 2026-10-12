#include <stdint.h>
#include "minemu/user_abi.h"
#include "shell.h"

void minemu_user_main(void) {
    shell_run();
    for (;;) {
        __asm__ volatile("nop");
    }
}
