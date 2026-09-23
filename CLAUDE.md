# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Student repo for `minimum`, a teaching OS (Stevens CS492). It is a freestanding A32 (Cortex-A9, soft-float) kernel, plus user-mode support, that runs on the `minemu` emulator (v0.2.0, Platform ABI v2). The platform spec lives in the [minemu repo](https://github.com/rchtsang/minemu) (see `docs/platform/abi-v2.md`). The current assignment is HW1: UART output, interrupt-driven UART input, a line reader, and a shell that prints the `msh> ` prompt and supports `echo`.

## Toolchain / environment

The host is Windows. The `arm-none-eabi-*` toolchain, `just`, and `minemu` are expected to run inside the course container (`just docker`, which runs `rtsang1/cs492-stevens:latest` with the repo mounted at `/workspace`). Run build and test commands from the repo root in that environment. Set `MINEMU=/path/to/minemu` if `minemu` isn't on `PATH`.

## Commands

```sh
make                      # bootloader consistency check + kernel + user + kernel examples
make kernel               # kernel only -> kernel/build/minimum-kernel.elf
make image                # package -> image/build/minimum.img (rebuilds kernel/user as needed)
just test-all hw1         # build image, run every tests/hw1/*.toml
just test hw1 echo        # run a single test manifest (tests/hw1/echo.toml)
make -C kernel/examples/irq-context-switch   # build one example
make clean
minemu run image/build/minimum.img --boot-rom bootloader/bootloader.bin   # interactive TUI (starts paused; Space s to run, i to type to UART, :q to quit)
```

Everything compiles with `-Wall -Wextra -Werror -std=c11 -ffreestanding -nostdlib`, so any warning breaks the build. There is no libc (no newlib), only `libgcc`.

## Tests

Tests are black-box `minemu test` TOML manifests in `tests/hwN/`. Each one boots the packaged image, can inject UART input at a given tick (`[[inputs]]`), and asserts on UART0 output (`uart0_contains`) within `max_ticks`. HW1 tests: `hello-world` (UART0 prints "hello world"), `prompt` ("msh> "), and `echo` (input `echo goodbye world\n` at tick 300000). The untouched starter fails all of them. New homework tests go in sibling `tests/hwN/` directories, and no new Just recipes are needed.

## Architecture

**Boot flow:** Boot ROM (`bootloader/`, fixed firmware; don't modify `bootloader.bin`) → copies kernel segments from system ROM to RAM → jumps to `minemu_bootstrap` (`kernel/src/startup/boot.S`). The bootstrap runs at physical addresses. It sets up the SVC/IRQ/ABT/UND stacks, builds page tables at `0x40010000` (RAM direct-mapped at `0xc0000000` → `0x40000000`, and MMIO `0x10000000` identity-mapped non-executable), enables the MMU, sets VBAR to `minemu_vectors`, and jumps to higher-half `minemu_bootstrap_high`. That calls `minemu_kernel_main(const struct minemu_boot_info *)` in `kernel/src/core/main.c`, which is where student kernel work begins.

**Exceptions:** `kernel/src/startup/vectors.S` holds the vector table. Undefined, prefetch-abort, and data-abort entries are supplied: they build an 88-byte `struct minemu_trap_frame` (layout pinned by `_Static_assert`s in `minemu/trap.h`) and call `minemu_undefined_dispatch` / `minemu_abort_dispatch`. The SVC and IRQ vectors branch to `minemu_svc_trampoline` / `minemu_irq_trampoline`, which are **weak** fail-stop stubs in `kernel/src/runtime/exception.c`, along with weak `minemu_irq_dispatch` / `minemu_svc_dispatch`. HW1 overrides the IRQ pair by adapting `kernel/examples/irq-context-switch/` (`irq.S` trampoline + `minemu_irq_dispatch`). The dispatch returns the trap frame to resume, which enables context switching. IRQ dispatch must ack the device and write `MINEMU_INTERRUPT->eoi`.

**Fixed interfaces (HW1):** `minemu/platform.h` (MMIO addresses, register structs like `MINEMU_UART0`, `MINEMU_SYSTICK`, `MINEMU_INTERRUPT`, IRQ numbers), `minemu/trap.h`, `minemu/irq.h`, and the `minemu_irq_dispatch` boundary must not change. Console, buffer, line-reader, and shell interfaces are the student's design. Other headers: `block.h` (synchronous block driver; unit 0 = filesystem, unit 1 = swap), `debug.h` (`breakpoint(TAG)` pauses the emulator, TAG is a constant 0–65535), `trace.h` (`minemu_trace_event`).

**Kernel build layout (`kernel/Makefile`):** `src/runtime/` + `src/startup/` are archived into `build/libminemu_kernel.a`. That library is the supplied runtime shared with the examples. `src/core/main.o` links against it. When adding kernel sources:
- List new objects in `kernel/Makefile`. Only `src/core/`, `src/runtime/`, and `src/startup/` have pattern rules, so a new subdirectory needs its own rule.
- Put student code in the kernel link (core objects), not in the runtime archive, or it will also be linked into the examples.
- `image/Makefile` tracks kernel dependencies with wildcards over `src/core`, `src/runtime`, `src/startup`, and `include/minemu`. Sources in new directories won't trigger a re-package unless you add them there too.

**Examples:** `kernel/examples/<name>/` use `kernel/common.mk`. They are standalone kernels that define their own `minemu_kernel_main` and link the runtime library. Register new ones in the `EXAMPLES` list in `kernel/Makefile`.

**User space:** `user/lib` → `libminimum_user.a` (freestanding `string.h`/memory functions). Programs live in `user/prog/<name>/` with a two-line Makefile (`PROGRAM := ...` + `include ../../common/common.mk`) and must be added to `PROGRAMS` in `user/Makefile`. The user program and SVC example are for later assignments, not HW1.

**Image:** `image/minimum.toml` selects the kernel ELF and the user modules packed by `minemu image`.

All build output stays in the owning component's `build/` directory (gitignored).

For a per-file, symbol-level reference (trap-frame offsets, page-table layout, IRQ protocol, change recipes) see `docs/REPOSITORY_MAP.ai.md`. The human-oriented tour is `docs/REPOSITORY_GUIDE.md`. Keep both in sync when the structure changes.
