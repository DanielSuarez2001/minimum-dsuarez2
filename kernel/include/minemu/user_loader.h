#ifndef USER_LOADER_H
#define USER_LOADER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "minemu/boot.h"

#define USER_STACK_TOP UINT32_C(0x00800000)

void minemu_enter_user(uint32_t entry_vaddr, uint32_t stack_top) __attribute__((noreturn));

bool map_system_rom(const struct minemu_boot_info *boot_info);

bool user_range_valid(uint32_t vaddr, uint32_t length, bool writable);

const struct minemu_module_record *find_module(const struct minemu_boot_info *boot_info, const char *name, size_t name_length);

bool load_user_module(const struct minemu_boot_info *boot_info, const struct minemu_module_record *module);

#endif
