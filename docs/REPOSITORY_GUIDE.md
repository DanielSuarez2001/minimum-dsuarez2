# Repository Guide

A tour of every folder and file in this repository: what it is, why it exists,
and whether you are expected to change it.

`minimum` is a small teaching operating system for a 32-bit ARM (Cortex-A9)
machine. The machine is simulated by the `minemu` emulator, so everything
here is compiled with a cross-compiler (`arm-none-eabi-gcc`) and run inside
`minemu` rather than on your computer directly.

> Companion documents: [`../README.md`](../README.md) (quickstart and emulator
> controls), [`../tests/README.md`](../tests/README.md) (running tests), and
> [`REPOSITORY_MAP.ai.md`](REPOSITORY_MAP.ai.md) (a dense version of this guide
> for AI coding assistants).

---

## The big picture

Four separately built pieces are combined into one bootable system:

```text
 bootloader/bootloader.bin   ─┐   (64 KiB Boot ROM, supplied, never changes)
                              │
 kernel/ ──► minimum-kernel.elf ─┐
                                 ├─► image/ ──► minimum.img ──► minemu run / minemu test
 user/   ──► minimum-user.elf  ──┘            (system ROM image)
```

When the emulator powers on:

1. **Boot ROM** (`bootloader/`) runs first. It copies the kernel out of the
   packaged image into RAM, writes a small "boot info" record describing the
   machine, and jumps to the kernel.
2. **Kernel bootstrap** (`kernel/src/startup/boot.S`) sets up exception stacks,
   builds page tables, turns on the MMU, and moves the kernel to its
   "higher-half" virtual address (`0xc0000000` and up).
3. **`minemu_kernel_main`** (`kernel/src/core/main.c`) runs. It checks the
   boot info, prints `hello world`, turns on UART input interrupts, and starts
   the `msh` shell, which runs forever.
4. Hardware interrupts and exceptions enter through the vector table
   (`kernel/src/startup/vectors.S`) and are handed to C "dispatch" functions.
   When you type a key, the path is: UART0 → interrupt controller → IRQ
   trampoline (`core/irq_entry.S`) → dispatcher (`core/irq.c`) → UART handler
   (`drivers/uart.c`), which stores the byte for the shell to read.

---

## Directory tree

```text
minimum-dsuarez2/
├── README.md               Quickstart: prerequisites, build, run, test, TUI controls
├── CLAUDE.md               Instructions for Claude Code (AI assistant)
├── Makefile                Top-level build: runs every component's Makefile
├── justfile                Shortcut recipes: build, image, test, test-all, clean, docker
├── .gitignore              Keeps build output (build/, *.o, *.elf, *.img, …) out of git
├── docs/                   This guide and its AI-oriented companion
├── bootloader/             Supplied Boot ROM firmware (do not modify)
├── kernel/                 The operating system kernel (your main work area)
├── user/                   User-mode library and programs (later assignments)
├── image/                  Packages kernel + user programs into minimum.img
└── tests/                  Automated black-box tests run by minemu
```

Every component writes generated files into its **own** `build/` folder. Deleting
any `build/` folder (or running `make clean`) is always safe.

---

## Root files

| File | Purpose |
|---|---|
| `README.md` | The official quickstart. How to install tools, build, package, test, and run the emulator's text UI. Read this first. |
| `Makefile` | The main build entry point. `make` builds everything and checks the Boot ROM. `make image` packages the system. Other targets: `kernel`, `kernel-examples`, `user`, `bootloader`, `clean`. Accepts `MINEMU=/path/to/minemu`. |
| `justfile` | Recipes for the [`just`](https://github.com/casey/just) command runner. `just test hw1 echo` runs one test, `just test-all hw1` runs every HW1 test, and `just docker` starts the course's development container with this folder mounted at `/workspace`. |
| `.gitignore` | Ignores all generated files so only source is committed. |
| `CLAUDE.md` | Short guidance file read automatically by Claude Code. |

---

## `bootloader/`: Boot ROM firmware (supplied)

The very first code the emulated CPU runs after reset. It is **fixed course
firmware**. Your work uses the checked-in `bootloader.bin` as-is.

| File | Purpose |
|---|---|
| `bootloader.bin` | The canonical, checked-in 64 KiB Boot ROM image. Passed to `minemu` with `--boot-rom`. **Do not replace it.** |
| `src/start.S` | Reset vector table. On reset it enters supervisor mode with interrupts off, sets a temporary stack at physical `0x40007000`, and calls `boot_main`. Any other exception during boot loops forever at `fail_stop`. |
| `src/boot.c` | `boot_main`: reads the packed image header in system ROM (`0x08000000`), copies each kernel segment into RAM, zeroes its BSS, writes the 64-byte boot-info record at physical `0x40007000`, and jumps to the kernel's bootstrap entry. |
| `linker/bootloader.ld` | Places the firmware at address 0 in a 64 KiB ROM. Asserts that the firmware has no writable data or BSS (a ROM can't hold them). |
| `Makefile` | `all` builds `build/bootloader.bin`. `check` confirms that your local build matches the canonical `bootloader.bin` byte-for-byte (`make` runs this every time). `update` overwrites the canonical file and is for maintainers only. |
| `README.md` | Maintainer notes for rebuilding and updating the firmware. |

---

## `kernel/`: the operating system

This is where assignment work happens.

### Your code (Homework 1)

Everything in this section was written for HW1. It lives in four folders
under `kernel/src/`, plus three headers.

| File | Purpose |
|---|---|
| `core/main.c` | `minemu_kernel_main(boot_info)`, the kernel's C entry point. It validates the boot-info record (template code; leave it alone), prints `hello world\n`, calls `uart_init()`, and then runs `shell_run()` forever. |
| `core/irq_entry.S` | `minemu_irq_trampoline`, the code the CPU jumps to on every hardware interrupt. It saves the interrupted registers into a trap frame, reads the interrupt controller's CLAIM register to learn which device fired, calls the dispatcher, then restores the registers and returns. It's copied unchanged from `kernel/examples/irq-context-switch/irq.S`. |
| `core/irq.c` | `minemu_irq_dispatch`, which picks the handler for the device that fired, using a table (`irq_handlers[]`, currently just UART0). It then writes the device number to the controller's EOI register ("I'm done"). If no device was claimed, it returns without an EOI. |
| `drivers/uart.c` | The UART0 driver. `uart_putchar`/`uart_putstring` send text by waiting for the transmitter to be ready. `uart_rx_irq_handler` runs on every receive interrupt and moves all waiting bytes into a 256-byte ring buffer. `uart_getchar` waits for a byte from that buffer, switching interrupts off while it touches the buffer. `uart_init` turns on receive interrupts. |
| `shell/shell.c` | The `msh` shell (see [Shell behavior](#shell-behavior)). `shell_run` prints `msh> `, collects a line, and runs it. `shell_run_line` parses and executes one line. |
| `helpers/string.c` | Small string helpers, since there is no C library: `strlen`, `str_eq`, `str_start_trim` (skip leading spaces), `str_split_on_space`, `str_is_first_word`. |

| Header | Declares |
|---|---|
| `include/minemu/uart.h` | The UART driver functions above |
| `include/minemu/shell.h` | `shell_run`, `shell_run_line` |
| `include/string.h` | The string helpers above (included as `"string.h"`) |

#### Shell behavior

- Prints `msh> ` whenever it's ready for a command. What you type isn't echoed
  back, because the emulator's console already shows your keystrokes.
- A line ends at `\n`. Lines can be up to 20 characters.
- `0x08` and `0x7f` are backspace. Backspace on an empty line does nothing.
- If you type past 20 characters, the extra characters are discarded. If you
  backspace until you're back within 20, the line is usable again. If the line
  is still too long when you press Enter, the shell prints
  `Error: Input line too long, max is 20 characters` and runs nothing.
- Leading spaces are ignored. An empty or all-space line just shows the prompt
  again.
- `echo TEXT` prints `TEXT` (after skipping the spaces right after `echo`) and
  a newline. Plain `echo` prints an empty line.
- Any other command prints `command not found: COMMAND`, where COMMAND is the
  first word.

You can add more `.c` or `.S` files under `kernel/src/`. Every new object file
must be added to `kernel/Makefile` (see [Adding code](#adding-code) below).

### `kernel/src/startup/`: boot and exception entry (supplied)

| File | Purpose |
|---|---|
| `boot.S` | `minemu_bootstrap` runs at physical addresses with the MMU off. It sets up the stacks for each CPU mode (SVC, IRQ, abort, undefined), builds the initial page tables at physical `0x40010000`, enables the MMU, points the vector base at `minemu_vectors`, then jumps to `minemu_bootstrap_high`. That routine reloads the stacks at their virtual addresses and calls `minemu_kernel_main`. **Preserve this setup.** |
| `vectors.S` | The exception vector table. The undefined-instruction, prefetch-abort, and data-abort entries are complete. They save the CPU registers into a `struct minemu_trap_frame` and call a C dispatch function. The **SVC** (system call) and **IRQ** (hardware interrupt) slots jump to trampolines you supply. |

### `kernel/src/runtime/`: support routines (supplied)

These files, together with `startup/`, are bundled into
`build/libminemu_kernel.a`. The kernel and every example link against this library.

| File | Purpose |
|---|---|
| `exception.c` | `minemu_fail_stop` (halt forever), `minemu_panic`, plus **weak** default versions of `minemu_irq_trampoline`, `minemu_svc_trampoline`, `minemu_irq_dispatch`, `minemu_svc_dispatch`, `minemu_undefined_dispatch`, and `minemu_abort_dispatch`. They all just halt. Defining a function with the same name in your code replaces the default. |
| `block.c` | `minemu_block_read` / `minemu_block_write`. These run a synchronous disk transfer by polling the block controller, with interrupts briefly masked. Used in later assignments (filesystem and swap). |
| `memory.c` | Freestanding `memcpy`, `memset`, `memmove`, `memcmp`. There is no C library, and GCC may emit calls to these implicitly. |
| `trace.c` | `minemu_trace_event(value)` writes a 32-bit value to the emulator's trace device. It is a cheap way to mark progress when UART output isn't working yet. |

### `kernel/include/minemu/`: platform headers

Include these as `#include "minemu/<name>.h"`.

| Header | Purpose | Changeable? |
|---|---|---|
| `platform.h` | The machine's memory map and devices: base addresses, register layouts, and ready-made pointers (`MINEMU_UART0`, `MINEMU_UART1`, `MINEMU_SYSTICK`, `MINEMU_INTERRUPT`, `MINEMU_BLOCK`, `MINEMU_RNG`, `MINEMU_TRACE`), status/control bits, and IRQ numbers. | Fixed for HW1 |
| `trap.h` | `struct minemu_trap_frame`, the 88-byte saved-register record that every exception entry builds, plus exception ID constants. | Fixed for HW1 |
| `irq.h` | Declarations for the IRQ trampoline and `minemu_irq_dispatch`, plus `minemu_irq_enable()` / `minemu_irq_disable()`. | Fixed for HW1 |
| `syscall.h` | Declarations for the SVC trampoline and `minemu_svc_dispatch` (system calls, later assignments). | |
| `boot.h` | `struct minemu_boot_info` (what the Boot ROM hands the kernel) and the module-record structures that describe packaged user programs. | |
| `mmu.h` | Page-table entry bits, fault-cause codes, and inline helpers for the MMU control registers (TTBR0, enable, TLB flush, VBAR, fault status/address). The minemu MMU uses its own simple two-level format. | |
| `block.h` | Block-device driver API (unit 0 = filesystem, unit 1 = swap). | |
| `debug.h` | `breakpoint(TAG)` pauses the emulator at that line so you can inspect the machine. | |
| `trace.h` | Declares `minemu_trace_event`. | |
| `runtime.h` | Declares `minemu_panic`. | |

### `kernel/linker/kernel.ld`

Describes where each part of the kernel lives in memory:

- The bootstrap code is linked at **physical** `0x40008000`, because it runs before the MMU is on.
- Everything else is linked at **virtual** `0xc0030000` but loaded at physical `0x40030000`. Virtual = physical + `0x80000000`.
- It reserves 4 KiB stacks for SVC, IRQ, abort, and undefined modes, and exports both their virtual and physical top addresses to `boot.S`.

### `kernel/examples/`: small reference kernels

Each example is a complete, standalone kernel with its own `minemu_kernel_main`
that links against the same runtime library. Build one with
`make -C kernel/examples/<name>`.

| Example | What it demonstrates |
|---|---|
| `mmio-basics/` | Talking to devices directly: printing to UART0, seeding and reading the RNG, starting the SysTick timer, writing a trace event. |
| `irq-context-switch/` | A full IRQ path. `irq.S` provides `minemu_irq_trampoline`, which saves a trap frame, reads the interrupt controller's CLAIM register to learn which device fired, and calls `minemu_irq_dispatch`. `main.c` acknowledges the device, signals end-of-interrupt, and switches between two tasks by returning a different trap frame. Your `core/irq_entry.S` is a copy of this `irq.S`. |
| `svc-context-switch/` | The same idea for system calls (`svc #0`): `svc.S` trampoline + `minemu_svc_dispatch` switching tasks. For later assignments. |

### Kernel build files

| File | Purpose |
|---|---|
| `kernel/Makefile` | Builds `build/minimum-kernel.elf` from your objects in `CORE_OBJECTS` (`core/main.o`, `core/irq_entry.o`, `core/irq.o`, `drivers/uart.o`, `shell/shell.o`, `helpers/string.o`) + `build/libminemu_kernel.a` + libgcc. It has compile rules for `.c` files in `core/`, `drivers/`, `shell/`, `helpers/` and `runtime/`, and for `.S` files in `core/` and `startup/`. Also lists the examples (`make -C kernel examples`). All warnings are errors (`-Werror`). |
| `kernel/common.mk` | Shared build rules used by each example's two-line Makefile: compile every `.c`/`.S` in the folder and link it with the runtime library. |

---

## `user/`: user-mode programs (later assignments)

Programs that will eventually run in unprivileged user mode on top of the
kernel. Not part of Assignment 1.

| Path | Purpose |
|---|---|
| `Makefile` | Builds the library, then every program listed in `PROGRAMS`. |
| `common/common.mk` | Shared build rules for programs: compile every source in the folder and link it with `libminimum_user.a` and libgcc. |
| `common/user.ld` | User linker script. Programs start at virtual `0x00400000`, and the entry point is `minemu_user_main`. |
| `lib/` | Source for `libminimum_user.a`, the user-space support library. |
| `lib/include/string.h` | Declares `memcpy`, `memset`, `memmove`, `memcmp`. |
| `lib/src/memory.c` | Implements them (the same code as the kernel's `memory.c`, built separately for user mode). |
| `prog/minimum-user/` | The starter user program: `main.c` defines `minemu_user_main`, which loops forever. |

---

## `image/`: packaging

| File | Purpose |
|---|---|
| `minimum.toml` | Manifest that tells `minemu image` which kernel ELF to use and which user programs (`[[modules]]`) to pack in. Edit it to add or remove user programs. |
| `Makefile` | Runs `minemu image minimum.toml --output build/minimum.img`. It rebuilds the kernel and user program first if their sources changed. `KERNEL_INPUTS` lists the kernel folders it watches: `core`, `drivers`, `shell`, `helpers`, `runtime`, `startup`, `include` and `include/minemu`. |

The output `image/build/minimum.img` is what you boot and test.

---

## `tests/`: automated tests

| Path | Purpose |
|---|---|
| `README.md` | How to run tests and where to add your own. |
| `hw1/hello-world.toml` | Passes if UART0 prints `hello world`. |
| `hw1/prompt.toml` | Passes if UART0 prints the shell prompt `msh> `. |
| `hw1/echo.toml` | Types `echo goodbye world` + Enter into UART0 at tick 300000, then passes if `goodbye world` appears. |

Each test is a `minemu test` manifest. It boots the packaged image, optionally feeds
input into a UART at a given time, and checks the output. The tests treat the system as a
black box, so they never call your functions directly. Run with `just test-all hw1`
or `just test hw1 <name>`. Later homework gets sibling folders (`hw2/`, …).

The current kernel passes all three HW1 tests. They only check that certain text
appears, so shell details such as backspace, the 20-character limit, overlong
lines and unknown commands need extra manifests of your own to test.

---

## Memory map at a glance

| Address | What's there |
|---|---|
| `0x00000000` | Boot ROM (64 KiB) |
| `0x08000000` | System ROM, holding the packaged `minimum.img` |
| `0x10000000` | Interrupt controller |
| `0x10001000` | SysTick timer |
| `0x10002000` | Block (disk) controller |
| `0x10003000` | Random number generator |
| `0x10004000` / `0x10005000` | UART0 / UART1 |
| `0x1000f000` | Trace device |
| `0x40000000` | RAM (64 MiB, physical) |
| `0x40007000` | Boot-info record (physical); the kernel sees it at `0xc0007000` |
| `0x40010000` | Initial page tables built by `boot.S` |
| `0xc0000000` | Kernel's view of all RAM (direct map of `0x40000000`) |
| `0xc0030000` | Kernel code and data start |
| `0x00400000` | User program start (virtual) |

---

## Adding code

- **New kernel source file:** put it under `kernel/src/` and add its `.o` to
  `CORE_OBJECTS` in `kernel/Makefile`. Don't add it to the runtime library,
  which is shared with the examples. The existing folders (`core/`,
  `drivers/`, `shell/`, `helpers/`) already have compile rules; a new folder
  needs its own rule, plus a wildcard in `KERNEL_INPUTS` in `image/Makefile`.
  Without that wildcard, running `make` inside `image/` directly won't notice
  your changes. Running `make image` from the repo root is always fine.
- **New interrupt source** (e.g. SysTick or UART1): enable it at the device
  and in `MINEMU_INTERRUPT->enable`, and add its handler to `irq_handlers[]`
  in `core/irq.c`. The handler must clear the device's request (read the data
  or write its ACK) before the dispatcher writes EOI.
- **New shell command:** add a branch next to the `echo` check in
  `shell_run_line` (`shell/shell.c`).
- **Replacing a weak default** (as `core/irq_entry.S` and `core/irq.c` do for
  the IRQ trampoline and dispatcher): define the function with the exact same
  name in your code. The linker prefers yours.
- **New kernel example:** create `kernel/examples/<name>/` with a Makefile
  containing `PROGRAM := <name>` and `include ../../common.mk`, then add it to
  `EXAMPLES` in `kernel/Makefile`.
- **New user program:** create `user/prog/<name>/` with a Makefile containing
  `PROGRAM := <name>` and `include ../../common/common.mk`, add it to
  `PROGRAMS` in `user/Makefile`, and add a `[[modules]]` entry to
  `image/minimum.toml`.
- **New test:** add a `.toml` manifest under `tests/hwN/`.
