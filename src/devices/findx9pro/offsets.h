/* OPPO Find X9 Pro (PLG110) — MediaTek Dimensity 9400 (MT6993), kernel 6.12
 * Rust ashmem, PSELECT_SHIFT=0, Path B only (off_ashmem_misc_fops unavailable).
 *
 * Verified on hardware: uname -r 6.12.23-android16-5-g82efd98459a2-ab14457512-4k,
 * ro.soc.model MT6993, ro.board.platform mt6993, ro.product.device OP5E11L1,
 * Android 16, security patch 2026-08-01.
 *
 * kernel_phys_load=0x80000000: /proc/iomem reports "Kernel code" at 0x80010000,
 * which is _stext, with _text 0x10000 lower. The .phys_offset field is left 0,
 * which select_offsets() reads as "keep target.h's P0_PHYS_OFFSET" — the same
 * 0x80000000 — so p0_data_alias() computes a zero delta, the form the image's
 * text_offset=0 supports. The two values only ever appear as a difference.
 *
 * off_security_hook_heads=0 and off_ashmem_misc_fops=0 are deliberate: this
 * kernel uses LSM static calls (no security_hook_heads list) and the Rust
 * ashmem driver has no static miscdevice fops table in BSS. Callers must test
 * them for zero before use — see runtime_struct_offsets.h _RSO_RAW. */

OFFSETS_ENTRY("6.12.23-android16-5-g82efd98459a2-ab14457512-4k",
  .kernel_phys_load=0x80000000, STRUCT_OFFSETS_6_12,
  .off_init_task=0x023ECF00, .off_init_cred=0x02402A68, .off_init_uts_ns=0x02574650,
  .off_empty_zero_page=0x02614000, .off_root_task_group=0x0261C580,
  .off_selinux_enforcing=0x026684F0, .off_kptr_restrict=0x023EB638,
  .off_selinux_blob_sizes=0x0182F4E8, .off_security_hook_heads=0,
  .off_kmalloc_caches=0x018294C0, .off_anon_pipe_buf_ops=0x0120ED88,
  .off_ashmem_misc_fops=0, .off_ashmem_fops=0x02695A88,
  .off_ashmem_ioctl=0x00D7E5A8, .off_ashmem_compat_ioctl=0x00D7EB90,
  .off_ashmem_mmap=0x00D7EC0C, .off_ashmem_open=0x00D7EC68,
  .off_ashmem_release=0x00D7E668, .off_ashmem_show_fdinfo=0x00D7EB68,
  .off_configfs_read_iter=0x00511A7C, .off_configfs_bin_write_iter=0x00512028,
  .off_copy_splice_read=0x0048E034, .off_noop_llseek=0x0043B678,
  .off_cap_capable_active=0x02662B50,
  .off_slide_nfulnl_logger=0x023E2198, .off_slide_loggers_0_1=0x023E20E8,
  .off_slide_boot_id=0x02689868,
  .off_system_unbound_wq=0x01829250, .off_call_usermodehelper_exec_work=0x000F6744,
),
