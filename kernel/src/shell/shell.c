#include <stdint.h>
#include "minemu/shell.h"
#include "minemu/uart.h"
#include "string.h"

static char shell_line_buffer[21]; // Buffer for the current line of input, 20 chars + null terminator
static uint32_t shell_line_head = 0;

static bool shell_is_overflowed = false; // Keeps track of line buffer has overflow
static uint32_t shell_overflow_count = 0; // Keeps track of overflow amount

void shell_run(void) {
   
    for (;;) {
        uart_putstring("msh> ");
        while (1) {
            char c = uart_getchar();
            if (c == '\n') { //Handle newline character
                if (shell_is_overflowed) {
                    uart_putstring("Error: Input line too long, max is 20 characters\n");
                    shell_line_head = 0; // Reset the line buffer
                    shell_is_overflowed = false; // Reset overflow flag
                    shell_overflow_count = 0; // Reset overflow count
                    break; // Exit the inner loop to prompt for a new line
                }
                shell_line_buffer[shell_line_head] = '\0'; // Null-terminate the string
                shell_run_line(shell_line_buffer);
                shell_line_head = 0; // Reset the line buffer
                break; // Exit the inner loop to prompt for a new line
            }
            else if (c == 0x08 || c == 0x7f ) { // Handle backspace 
                if (shell_line_head > 0) {
                    if (shell_is_overflowed) {
                        shell_overflow_count--;
                        if (shell_overflow_count == 0) {
                            shell_is_overflowed = false;
                        }
                    }
                    else {
                        shell_line_head--;
                    }
                }
            }
            else if (shell_line_head < sizeof(shell_line_buffer) - 1) {
                shell_line_buffer[shell_line_head++] = c;
            }
            else {
                shell_is_overflowed = true;
                shell_overflow_count++;
            }
        }
    }
}

 // Process the line of input from the shell
void shell_run_line(char *line) {
    //Remove leading white space
    line = str_start_trim(line);

    // If the line is empty after trimming, just return to prompt
    if (line[0] == '\0') {
        return;
    }

    // Split the line on the first space to separate command and arguments
    char *suffix = str_split_on_space(line);
    if (suffix) {
        // Trim leading whitespace from suffix
        suffix = str_start_trim(suffix);
    }
    // Check for known commands
    if (str_eq(line, "echo")) {
        if (suffix) {
            uart_putstring(suffix);
        }
    }
    else {
        uart_putstring("command not found: ");
        uart_putstring(line);
    }
    uart_putstring("\n");
}

