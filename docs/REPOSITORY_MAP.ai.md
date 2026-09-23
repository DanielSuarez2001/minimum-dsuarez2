# REPOSITORY_MAP (AI reference)

A dense, symbol-level map of this repo for coding agents. The human-oriented version is
[`REPOSITORY_GUIDE.md`](REPOSITORY_GUIDE.md). Facts were verified against the source; if they
disagree with a file, the file wins.

## Identity

- Project: `minimum`, a teaching OS (Stevens CS492), student copy `minimum-dsuarez2`.
- Target: ARMv7-A A32, `-mcpu=cortex-a9 -marm -mfloat-abi=soft`, freestanding, `-nostdlib`, libgcc only, no newlib.
- Runtime: the `minemu` 0.2.0 emulator (Platform ABI v2 docs: github.com/rchtsang/minemu `docs/platform/abi-v2.md`). The boot-info `version` field is still `1` (`MINEMU_ABI_VERSION`).
- Current assignment: HW1. Needs UART0 output, interrupt-driven UART0 input, a line reader, and a shell with the prompt `msh> ` and an `echo` command.
- Compiler flags include `-Wall -Wextra -Werror -std=c11 -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -fno-stack-protector`, linked with `--gc-sections`.
- Host is Windows. The toolchain normally runs in the container (`just docker` → `rtsang1/cs492-stevens:latest`, repo at `/workspace`).

## Commands

| Goal | Command |
|---|---|
| Build all + Boot ROM check | `make` |
| Kernel only | `make kernel` → `kernel/build/minimum-kernel.elf` (+ `.map`) |
| Package | `make image` → `image/build/minimum.img` (depends on kernel+user) |
| All HW1 tests | `just test-all hw1` (builds image first) |
| One test | `just test hw1 <echo\|hello-world\|prompt>` |
| One example | `make -C kernel/examples/<name>` |
| Run TUI | `minemu run image/build/minimum.img --boot-rom bootloader/bootloader.bin` |
| Clean | `make clean` |
| Override emulator | `MINEMU=/path make image` / `MINEMU=/path just test-all hw1` |

## File index

Legend: **E** = student-editable/expected work area, **F** = fixed/supplied (do not modify), **G** = generated (gitignored), **L** = later assignment.

### Root
| Path | Tag | Role |
|---|---|---|
| `Makefile` | F | `all: bootloader-check kernel user kernel-examples`; `image: kernel user`; passes `MINEMU`. |
| `justfile` | F | `build`, `image`, `test hw name`, `test-all hw`, `clean`, `docker`. `test-all` loops over `tests/<hw>/*.toml` and returns nonzero if any test fails. |
| `README.md` | F | Quickstart, TUI keys, where-to-work rules. |
| `.gitignore` | F | `**/build/ *.o *.d *.a *.elf *.map *.img .DS_Store`. |
| `CLAUDE.md` | E | Agent guidance. |
| `docs/` | E | This file + human guide. |

### bootloader/ (all F)
| Path | Role |
|---|---|
| `bootloader.bin` | Canonical 64 KiB Boot ROM; `make` runs `check` which `cmp`s a fresh build against it. Never run `make -C bootloader update`. |
| `src/start.S` | Vectors at 0x0: reset→`reset` (SVC mode, IRQ/FIQ masked, sp=0x40007000)→`boot_main`; all other vectors→`fail_stop` loop. |
| `src/boot.c` | `boot_main`: parses the packed image header at 0x08000000 (offsets: +0x08 image_size, +0x0c kernel seg table, +0x10 count, +0x14 module table, +0x18 module count, +0x1c bootstrap entry); copies each 32-byte segment record's `file_size` to its `paddr` and zeroes up to `memory_size`; writes boot_info at phys 0x40007000; calls entry(r0 = 0xc0007000, a virtual pointer that isn't mapped yet). |
| `linker/bootloader.ld` | ROM at 0, 64K; asserts `.data`/`.bss` empty. |
| `Makefile` | `all`, `check` (entry=0, no undefined syms, size 65536, no Thumb, byte-identical), `update`, `clean`. `-Os -Wstack-usage=512`. |
| `README.md` | Maintainer guide. |

### kernel/
| Path | Tag | Role / key symbols |
|---|---|---|
| `src/core/main.c` | E | `void minemu_kernel_main(const struct minemu_boot_info *)`. The starter validates boot_info (addr, magic 0x4d424f4f, version, size 64, rom 0x08000000, direct map 0xc0000000↔0x40000000 size 0x04000000); on mismatch it traces 0xb007bad0; otherwise traces 1, then `minemu_fail_stop()`. |
| `src/startup/boot.S` | F | `minemu_bootstrap` (section `.bootstrap.text`, phys 0x40008000; forced kept via `-Wl,-u,minemu_bootstrap`): saves r0 in r8, masks IRQ/FIQ, sets SVC/IRQ/ABT/UND sp to `__minemu_*_stack_top_paddr`, `bl minemu_bootstrap_map`, TTBR0=0x40010000, TLBIALL, SCTLR=1 (MMU on), VBAR=`minemu_vectors`, `bx minemu_bootstrap_high`. `minemu_bootstrap_high` (`.text.startup`, ELF ENTRY) reloads virtual stack tops, `r0=r8`, `bl minemu_kernel_main`, then `bl minemu_fail_stop`. The kernel is entered in **SVC mode with IRQ+FIQ masked**. |
| `src/startup/vectors.S` | F | `minemu_vectors` (`.vectors`, VA 0xc0030000): [0] reset→`minemu_fail_stop`, [1] undef→`minemu_undefined_entry`, [2] svc→`minemu_svc_trampoline`, [3] pabt→`minemu_prefetch_abort_entry`, [4] dabt→`minemu_data_abort_entry`, [5] 0, [6] irq→`minemu_irq_trampoline`, [7] 0 (FIQ unhandled). Undef/abort entries build a trap frame on the mode stack, call `minemu_undefined_dispatch`/`minemu_abort_dispatch` (void return, same frame resumed). Return: undef `movs pc, lr`; pabt `subs pc, lr, #4`; dabt `subs pc, lr, #8` (re-executes the faulting instruction). |
| `src/runtime/exception.c` | F | Strong: `minemu_fail_stop` (nop loop), `minemu_panic(msg)` (ignores msg → fail_stop). **Weak** (all fail_stop): `minemu_svc_trampoline`, `minemu_irq_trampoline`, `minemu_svc_dispatch`, `minemu_irq_dispatch`, `minemu_undefined_dispatch`, `minemu_abort_dispatch`. Override by defining a strong symbol in the kernel link. |
| `src/runtime/block.c` | F/L | `minemu_block_read/write(unit, lba, sector_count, dma_paddr)`: saves CPSR, `cpsid i`, programs `MINEMU_BLOCK` regs, spins on STATUS_COMPLETE, reads error, ACKs, restores I only if it was clear. Returns `MINEMU_BLOCK_ERROR_*`. DMA needs a 512-byte-aligned **physical** address. |
| `src/runtime/memory.c` | F | `memcpy memset memmove memcmp` (byte loops). Needed because GCC may emit implicit calls. |
| `src/runtime/trace.c` | F | `minemu_trace_event(v)` → `MINEMU_TRACE->event = v`. |
| `include/minemu/platform.h` | F (HW1) | Memory map + MMIO structs + `MINEMU_UART0/UART1/SYSTICK/INTERRUPT/BLOCK/RNG/TRACE` volatile pointers + bit constants + IRQ ids (SYSTICK 0, UART0 1, UART1 2, BLOCK 3, NONE 0xffffffff). |
| `include/minemu/trap.h` | F (HW1) | `struct minemu_trap_frame` (88 B, see below), `MINEMU_EXCEPTION_{UNDEFINED -4, SVC -3, PREFETCH_ABORT -2, DATA_ABORT -1}`, dispatch prototypes, `minemu_fail_stop` noreturn. |
| `include/minemu/irq.h` | F (HW1) | `minemu_irq_trampoline` (noreturn), `minemu_irq_dispatch(frame) → frame*`, inline `minemu_irq_enable()` (`cpsie i`) / `minemu_irq_disable()` (`cpsid i`). |
| `include/minemu/syscall.h` | L | `minemu_svc_trampoline`, `minemu_svc_dispatch(frame) → frame*`. |
| `include/minemu/boot.h` | F | `struct minemu_boot_info` (64 B), `minemu_module_record` (32 B), `minemu_module_segment` (32 B), `MINEMU_BOOT_INFO_{MAGIC,VADDR}`, `MINEMU_ABI_VERSION` (1), `MINEMU_SEGMENT_{READABLE,WRITABLE,EXECUTABLE}`. Module/segment offsets are relative to system ROM base. |
| `include/minemu/mmu.h` | L | minemu's custom 2-level MMU (not ARM short-descriptor). PTE bits: VALID 1, WRITABLE 2, USER 4, EXECUTABLE 8, READABLE 16, ACCESSED 32, DIRTY 64, SW mask 0xf80, page mask 0xfffff000. PDE = page-table paddr \| VALID. Fault codes in DFSR low byte: TRANSLATION 1, READ_PROT 2, WRITE_PROT 3, EXEC_PROT 4, DEVICE_ACCESS 5; flags FROM_USER 0x100, IS_WRITE 0x200, IS_FETCH 0x400. Helpers: `set_ttbr0`, `set_enabled`, `tlbiall`, `set_vbar`, `dfsr`, `dfar`. |
| `include/minemu/block.h` | L | Block API prototypes. |
| `include/minemu/debug.h` | F | `breakpoint(tag)` → `bkpt #tag` (tag must be an immediate 0–65535). |
| `include/minemu/trace.h`, `runtime.h` | F | Declare `minemu_trace_event`, `minemu_panic`. |
| `linker/kernel.ld` | F | See memory layout. ENTRY `minemu_bootstrap_high`. PHDRs: `bootstrap` (R+X), `kernel` (RWX). |
| `Makefile` | E | `CORE_OBJECT := build/core/main.o`; `RUNTIME_OBJECTS` (runtime + startup) → `build/libminemu_kernel.a`; link = core + archive + libgcc. Pattern rules only for `src/core/%.c`, `src/runtime/%.c`, `src/startup/%.S`. `EXAMPLES` list. Targets: `all`, `runtime`, `examples`, `<example>`, `clean`. |
| `common.mk` | F | Example build: every `*.c`/`*.S` in the cwd + `libminemu_kernel.a` + kernel.ld → `build/<PROGRAM>.elf`. |
| `examples/mmio-basics/` | F | Polled UART0 tx (no TX_READY check), RNG seed/read→trace, SysTick enable (no IRQ), block IRQ enable, fail_stop. |
| `examples/irq-context-switch/irq.S` | F (template for HW1) | Strong `minemu_irq_trampoline`: builds frame, **exception_id = `MINEMU_INTERRUPT->claim`** (load from 0x10000008), `fault_pc = lr-4`, calls `minemu_irq_dispatch`, **`mov sp, r0`** (switch to returned frame), restores, `subs pc, lr, #4`. |
| `examples/irq-context-switch/main.c` | F (template) | Dispatch: ack the source device (SysTick `ack`; UART `rx_data` read; block `ack`), then `MINEMU_INTERRUPT->eoi = source`; 2-task switch by returning a different frame. Main: enable SysTick IRQ in the controller, period 100, periodic+IRQ, `cpsie i`, spin. |
| `examples/svc-context-switch/{svc.S,main.c}` | L | Same pattern for `svc`: exception_id = -3, return `movs pc, lr` (no adjust). `svc.S` has a stale unused `.extern minemu_svc_select`. |

### user/ (L)
| Path | Role |
|---|---|
| `Makefile` | `lib` then `PROGRAMS := minimum-user`. |
| `common/common.mk` | Program build; includes `-I user/lib/include -I kernel/include`; links `libminimum_user.a` + libgcc with `user.ld`. |
| `common/user.ld` | Base VA 0x00400000, ENTRY `minemu_user_main`, sections text/rodata/data/bss. |
| `lib/include/string.h`, `lib/src/memory.c` | User copies of `mem*` (duplicate of the kernel's `memory.c`). `lib/Makefile` builds every `src/*.c` → `build/libminimum_user.a`. |
| `prog/minimum-user/{main.c,Makefile}` | `minemu_user_main` nop loop. |

### image/
| Path | Tag | Role |
|---|---|---|
| `minimum.toml` | E | `kernel = "../kernel/build/minimum-kernel.elf"`; `[[modules]] name="minimum-user" elf=...`. |
| `Makefile` | E | `minemu image minimum.toml --output build/minimum.img`. Rebuild triggers: `KERNEL_INPUTS` = wildcards over `kernel/src/{core,runtime}/*.c`, `kernel/src/startup/*.S`, `kernel/include/minemu/*.h`, kernel.ld, kernel Makefile; `USER_INPUTS` similarly for minimum-user + lib. |

### tests/
| Path | Role |
|---|---|
| `hw1/hello-world.toml` | max_ticks 500000; assert `uart0_contains = "hello world"`. |
| `hw1/prompt.toml` | max_ticks 500000; assert `"msh> "`. |
| `hw1/echo.toml` | max_ticks 1000000; input `"echo goodbye world\n"` on uart 0 at tick 300000; assert `"goodbye world"`. |
| All | `execution_lifecycle = "paused"`, `shutdown_lifecycle = "stopped"`; paths are relative to the manifest (`../../image/build/minimum.img`, `../../bootloader/bootloader.bin`). |
| `README.md` | Test usage; students may add manifests anywhere under `tests/`. |

## Trap frame (`struct minemu_trap_frame`, 88 bytes, 4-aligned)

| Off | Field | Filled with |
|---|---|---|
| 0–48 | `r[13]` | r0–r12 |
| 52 | `return_lr` | banked lr of the exception mode (raw; the return path applies the per-vector adjust) |
| 56 | `spsr` | interrupted CPSR |
| 60 | `exception_id` | negative `MINEMU_EXCEPTION_*`, or the **IRQ number from CLAIM** for IRQs |
| 64 | `fault_pc` | lr-4 (undef, svc, pabt, irq) or lr-8 (dabt) |
| 68 | `dfsr` | CP15 DFSR for aborts, else 0 |
| 72 | `dfar` | CP15 DFAR for aborts, else 0 |
| 76 | `user_sp` | user-mode banked sp (`stmia {sp,lr}^`) |
| 80 | `user_lr` | user-mode banked lr |
| 84 | `reserved` | 0 |

Offsets 52/56/60/64/76 are hard-coded in the assembly and pinned by `_Static_assert`. Do not reorder.

**Resuming a synthesized frame:** the IRQ path returns via `subs pc, lr, #4`, so a new task's `return_lr` must be `entry + 4` (see irq example). The SVC path returns via `movs pc, lr`, so there `return_lr = entry`. Set `spsr` to a valid mode/flags value (the examples copy the current frame's spsr). The frame lives on the exception-mode stack or in caller-provided storage. The trampoline does `mov sp, r0`, so the returned frame pointer becomes the IRQ/SVC stack pointer during restore.

## Memory layout

Physical:
- `0x00000000` Boot ROM 64K · `0x08000000` system ROM 16M (packed image)
- MMIO pages: `0x10000000` INTC · `0x10001000` SysTick · `0x10002000` Block · `0x10003000` RNG · `0x10004000` UART0 · `0x10005000` UART1 · `0x1000f000` Trace
- RAM `0x40000000`–`0x43ffffff` (64 MiB). The Boot ROM temp stack top is `0x40007000`, boot_info is at `0x40007000`, bootstrap code at `0x40008000`, and the page directory at `0x40010000`. Page tables: `0x40011000` (low RAM identity), `0x40012000`–`0x40021fff` (16 tables, direct map), `0x40022000` (MMIO). The kernel image is loaded at `0x40030000`.

Virtual (set up by `minemu_bootstrap_map`; PDE index = VA>>22):
- `0x40000000`–`0x403fffff` identity → RAM (first 4 MiB; PTE 0x1b = V|W|X|R). Still mapped after boot.
- `0xc0000000`–`0xc3ffffff` → `0x40000000` (all RAM, PTE 0x1b). Kernel VA = PA + 0x80000000 (`kernel.ld` uses `AT(ADDR - 0x80000000)`).
- `0x10000000`–`0x1000ffff` identity → MMIO (PTE 0x13 = V|W|R, non-executable). So the `MINEMU_*` physical-address pointers work unchanged after the MMU is on.
- The kernel image starts at `.vectors` VA `0xc0030000`, then `.text` (`.text.startup` first), `.rodata`, `.data`, `.bss`, then `.stacks`: SVC, IRQ, ABT, UND at 4 KiB each, 8-aligned, no guard pages.
- User programs link at VA `0x00400000` (no user mappings exist yet).

## Interrupt controller protocol (from the irq example)

1. Enable the source: `MINEMU_INTERRUPT->enable |= 1u << MINEMU_IRQ_X` (mask 0x0f). Device-side: UART `control = MINEMU_UART_CONTROL_RX_IRQ_ENABLE`; SysTick `control |= IRQ_ENABLE`; Block `control = IRQ_ENABLE`.
2. Unmask the CPU: `minemu_irq_enable()`. The kernel starts masked.
3. On entry the trampoline reads `claim`. Dispatch must quiet the device (UART: read `rx_data` until `status & RX_READY` is clear; SysTick: write `ack`; Block: write `ack`) and then write `eoi = source`.
4. Priorities are per-source registers (reset: SysTick 0, UART0/1 64, Block 128). A claim can return `MINEMU_IRQ_NONE`.

UART: `status` bit0 RX_READY, bit1 TX_READY; write `tx_data` for output (TX FIFO 8192 bytes, RX FIFO 4096 bytes).

## Invariants / do-not-touch

- Don't modify `bootloader/**` or `bootloader.bin`, or `make` fails the `cmp` check.
- Preserve `vectors.S`, `boot.S`, the stack setup, and the page-table setup.
- For HW1, keep `platform.h`, `trap.h`, `irq.h`, and the `minemu_irq_dispatch` signature unchanged.
- Symbols that must keep their names: `minemu_kernel_main`, `minemu_irq_trampoline`, `minemu_irq_dispatch`, `minemu_svc_trampoline`, `minemu_svc_dispatch`, `minemu_undefined_dispatch`, `minemu_abort_dispatch`, `minemu_fail_stop`, `minemu_bootstrap`, `minemu_bootstrap_high`, `minemu_vectors`, `minemu_user_main`.
- There is no libc. Headers available: freestanding `<stdint.h>`, `<stddef.h>`, `<stdbool.h>`, `<stdarg.h>` + `minemu/*`. No `printf`/`malloc`/`strlen` (write your own). Division and modulo resolve through libgcc.
- The shell prompt string is exactly `msh> ` (trailing space).

## Change recipes

- **Add a kernel source** `kernel/src/<dir>/<f>.c`: append `build/<dir>/<f>.o` to the kernel link. Either add it to `CORE_OBJECT`, or better, introduce a `KERNEL_OBJECTS` list and update the `$(KERNEL)` rule and `OBJECTS`. Add a pattern rule `$(BUILD)/<dir>/%.o: src/<dir>/%.c` if `<dir>` is new. Add `$(wildcard ../kernel/src/<dir>/*.c)` to `KERNEL_INPUTS` in `image/Makefile`. **Don't** add it to `RUNTIME_OBJECTS`: that archive links into every example, and strong IRQ/dispatch overrides there would collide with the examples' definitions.
- **Adopt the IRQ trampoline:** copy `kernel/examples/irq-context-switch/irq.S` into `kernel/src/` (e.g. `src/core/irq.S`, which needs a `%.S` rule for `core`, plus `$(wildcard ../kernel/src/core/*.S)` in `image/Makefile` `KERNEL_INPUTS`, since only `core/*.c` is watched), and implement `minemu_irq_dispatch` in C.
- **New example:** `kernel/examples/<n>/Makefile` = `PROGRAM := <n>` + `include ../../common.mk`, and add `<n>` to `EXAMPLES`.
- **New user program:** `user/prog/<n>/Makefile` = `PROGRAM := <n>` + `include ../../common/common.mk`, add it to `PROGRAMS` in `user/Makefile`, add a `[[modules]]` entry to `image/minimum.toml`, and extend `image/Makefile` inputs if you want auto-rebuild.
- **New test:** `tests/hwN/<name>.toml` using the same keys as the hw1 manifests. `just test hwN <name>` picks it up.

## Debugging aids

- `minemu_trace_event(u32)` writes to the trace device (it works before the UART does).
- `breakpoint(TAG)` pauses the emulator at `bkpt`. Resuming steps past it.
- TUI: `Space s` run/pause, `i` type into UART, `Space i` inspect, `?` help, `:q` quit.
- Link maps: `kernel/build/minimum-kernel.map`, `kernel/examples/<n>/build/<n>.map`.
- A fault with no handler lands in `minemu_fail_stop` (nop loop). Pause and check PC/LR.
