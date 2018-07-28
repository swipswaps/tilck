
#pragma once

/* Utils */

#define KB (1024)
#define MB (1024*1024)

#include <generated_config.h>

#define KMALLOC_FREE_MEM_POISON_VAL 0xFAABCAFE

/* ------------------------------- */

/*
 * For the moment, keep those defines always defined.
 * Unless qemu is run with -device isa-debug-exit,iobase=0xf4,iosize=0x04
 * debug_qemu_turn_off_machine() won't turn off the VM.
 */

#define DEBUG_QEMU_EXIT_ON_INIT_EXIT
#define DEBUG_QEMU_EXIT_ON_PANIC

#define KERNEL_INITIAL_STACK_SIZE (4 * KB)

#if defined(TESTING) || defined(KERNEL_TEST)

   extern void *kernel_va;

   /* For the unit tests, we need to override the following defines */
   #undef KERNEL_BASE_VA
   #undef LINEAR_MAPPING_MB

   #define KERNEL_BASE_VA             ((uptr)kernel_va)
   #define LINEAR_MAPPING_MB          (128)

#endif

#define LINEAR_MAPPING_SIZE        (LINEAR_MAPPING_MB << 20)
#define LINEAR_MAPPING_END         (KERNEL_BASE_VA + LINEAR_MAPPING_SIZE)

#define KMALLOC_MAX_ALIGN          (64 * KB)
#define KMALLOC_MIN_HEAP_SIZE      KMALLOC_MAX_ALIGN
#define KMALLOC_FIRST_HEAP_SIZE    (256 * KB)

#define KERNEL_MAX_SIZE            (1024 * KB)
#define USER_VSDO_LIKE_PAGE_VADDR  (LINEAR_MAPPING_END)

/* Bootloader-specific config */

#define SECTOR_SIZE          512
#define BL_BASE_SEG          (BL_BASE_ADDR / 16)