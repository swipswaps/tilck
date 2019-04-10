/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

/*
 * Tilck's virtual file system
 *
 * As this project's goals are by far different from the Linux ones, this
 * layer won't provide anything close to the Linux's VFS.
 */

#include <tilck/common/basic_defs.h>

#include <tilck/kernel/sys_types.h>
#include <tilck/kernel/sync.h>

typedef struct process_info process_info;

/*
 * Opaque type for file handles.
 *
 * The only requirement for such handles is that they must have at their
 * beginning all the members of fs_handle_base. Therefore, a fs_handle MUST
 * always be castable to fs_handle_base *.
 */
typedef void *fs_handle;

typedef struct filesystem filesystem;

/* fs ops */
typedef void (*func_close) (fs_handle);
typedef int (*func_open) (filesystem *, const char *, fs_handle *, int, mode_t);
typedef int (*func_dup) (fs_handle, fs_handle *);
typedef int (*func_getdents64) (fs_handle, struct linux_dirent64 *, u32);

typedef void (*func_fs_ex_lock)(filesystem *);
typedef void (*func_fs_ex_unlock)(filesystem *);
typedef void (*func_fs_sh_lock)(filesystem *);
typedef void (*func_fs_sh_unlock)(filesystem *);

/* file ops */
typedef ssize_t (*func_read) (fs_handle, char *, size_t);
typedef ssize_t (*func_write) (fs_handle, char *, size_t);
typedef off_t (*func_seek) (fs_handle, off_t, int);
typedef int (*func_ioctl) (fs_handle, uptr, void *);
typedef int (*func_stat) (fs_handle, struct stat64 *);
typedef int (*func_mmap) (fs_handle, void *vaddr, size_t);
typedef int (*func_munmap) (fs_handle, void *vaddr, size_t);
typedef int (*func_fcntl) (fs_handle, int, int);
typedef void (*func_ex_lock)(fs_handle);
typedef void (*func_ex_unlock)(fs_handle);
typedef void (*func_sh_lock)(fs_handle);
typedef void (*func_sh_unlock)(fs_handle);

typedef bool (*func_rwe_ready)(fs_handle);
typedef kcond *(*func_get_rwe_cond)(fs_handle);

/* Used by the devices when want to remove any locking from a file */
void vfs_file_nolock(fs_handle h);

#define VFS_FS_RO        (0)
#define VFS_FS_RW        (1 << 0)

struct filesystem {

   const char *fs_type_name; /* statically allocated: do NOT free() */

   u32 device_id;
   u32 flags;
   void *device_data;
   int ref_count;

   func_open open;
   func_close close;
   func_dup dup;
   func_getdents64 getdents64;

   /* Whole-filesystem locks */
   func_fs_ex_lock fs_exlock;
   func_fs_ex_unlock fs_exunlock;
   func_fs_sh_lock fs_shlock;
   func_fs_sh_unlock fs_shunlock;
};

typedef struct {

   /* mandatory */
   func_read read;
   func_write write;
   func_seek seek;
   func_ioctl ioctl;
   func_stat stat;
   func_mmap mmap;
   func_munmap munmap;
   func_fcntl fcntl;

   /* optional, r/w/e ready funcs */
   func_rwe_ready read_ready;
   func_rwe_ready write_ready;
   func_rwe_ready except_ready;       /* unfetched exceptional condition */
   func_get_rwe_cond get_rready_cond;
   func_get_rwe_cond get_wready_cond;
   func_get_rwe_cond get_except_cond;

   /* optional, per-file locks */
   func_ex_lock exlock;
   func_ex_unlock exunlock;
   func_sh_lock shlock;
   func_sh_unlock shunlock;

} file_ops;

typedef struct {

   filesystem *fs;
   u32 path_len;
   char path[0];

} mountpoint;

/*
 * Each fs_handle struct should contain at its beginning the fields of the
 * following base struct [a rough attempt to emulate inheritance in C].
 *
 * TODO: introduce a ref-count in the fs_base_handle struct when implementing
 * thread support.
 */

#define FS_HANDLE_BASE_FIELDS    \
   filesystem *fs;               \
   file_ops fops;                \
   int fd_flags;                 \
   int fl_flags;                 \

typedef struct {

   FS_HANDLE_BASE_FIELDS

} fs_handle_base;

int mountpoint_add(filesystem *fs, const char *path);
void mountpoint_remove(filesystem *fs);

int vfs_open(const char *path, fs_handle *out, int flags, mode_t mode);
int vfs_ioctl(fs_handle h, uptr request, void *argp);
int vfs_stat64(fs_handle h, struct stat64 *statbuf);
int vfs_dup(fs_handle h, fs_handle *dup_h);
int vfs_getdents64(fs_handle h, struct linux_dirent64 *dirp, u32 bs);
void vfs_close(fs_handle h);
int vfs_fcntl(fs_handle h, int cmd, int arg);

bool vfs_read_ready(fs_handle h);
bool vfs_write_ready(fs_handle h);
bool vfs_except_ready(fs_handle h);
kcond *vfs_get_rready_cond(fs_handle h);
kcond *vfs_get_wready_cond(fs_handle h);
kcond *vfs_get_except_cond(fs_handle h);

ssize_t vfs_read(fs_handle h, void *buf, size_t buf_size);
ssize_t vfs_write(fs_handle h, void *buf, size_t buf_size);
off_t vfs_seek(fs_handle h, s64 off, int whence);

static ALWAYS_INLINE filesystem *get_fs(fs_handle h)
{
   ASSERT(h != NULL);
   return ((fs_handle_base *)h)->fs;
}

/* Per-file locks */
void vfs_exlock(fs_handle h);
void vfs_exunlock(fs_handle h);
void vfs_shlock(fs_handle h);
void vfs_shunlock(fs_handle h);

/* Whole-filesystem locks */
void vfs_fs_exlock(filesystem *fs);
void vfs_fs_exunlock(filesystem *fs);
void vfs_fs_shlock(filesystem *fs);
void vfs_fs_shunlock(filesystem *fs);
/* --- */

int
compute_abs_path(const char *path, const char *cwd, char *dest, u32 dest_size);

u32 vfs_get_new_device_id(void);
fs_handle get_fs_handle(int fd);
void close_cloexec_handles(process_info *pi);
