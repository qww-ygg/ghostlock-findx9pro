# GhostLock — Locked Bootloader Jailbreak

Kernel exploit for Android devices with locked bootloader. Achieves temporary root + KernelSU installation without unlocking bootloader or modifying boot image.

**This fork targets one device: OPPO Find X9 Pro (PLG110 / MT6993 / Android 16).**
Upstream tracked many devices but only ever worked on the one baked into
`target.h` as its fallback, because `main.c` rebound its offsets to the runtime
table while `fops.c`, `util.c`, `slide.c` and `root.c` kept the compile-time
constants. That is fixed here — `runtime_struct_offsets.h` is now included at the
end of `common.h`, so every translation unit resolves through `active_offsets` —
and the offset table is reduced to the single entry that has been verified on
hardware. Adding a device back means adding one `src/devices/<name>/offsets.h`
and an include.

<p align="center">
  <img src="assets/screenshot.jpg" width="300" alt="GhostLock running on OnePlus Ace 6T with KernelSU (LKM, Jailbreak mode)">
</p>

## Vulnerability

**CVE-2026-43499** — Futex PI (Priority Inheritance) Use-After-Free

Affects Linux kernel 5.7 ~ 7.1. Fixed in stable 6.1.175, 6.6.140, 6.12.86. Most Android devices remain unpatched as of September 2026.

The `pselect6` syscall copies `fd_set` data onto the kernel stack. When combined with the futex PI waiter mechanism, a freed stack frame can be reclaimed as an `rt_mutex_waiter` structure. The rb-tree rebalance during PI chain walk then writes controlled values to arbitrary kernel addresses.

## Target Device

| Field | Value |
|-------|-------|
| Model | OPPO Find X9 Pro |
| Model code | PLG110 / OP5E11L1 |
| SoC | MediaTek Dimensity 9400 (MT6993), `ro.board.platform=mt6993` |
| Kernel | `6.12.23-android16-5-g82efd98459a2-ab14457512-4k` |
| KMI | `android16-6.12` |
| Android | 16, security patch 2026-08-01 |
| SHIFT | 0 (`do_futex` not inlined — feasible) |

`select_offsets()` matches on `uname -r` exactly, so a kernel update on the same
model produces "unknown kernel" rather than a wrong write. That is deliberate: a
mismatched offset on this exploit does not fault, it silently writes to unrelated
RAM.

### Why this table has one entry

Upstream listed 15 entries, but every one of them except the fallback device in
`target.h` was inert. `main.c` had 28 `#undef X_OFF` lines rebinding the per-device
symbol macros to `active_offsets->*`, while `fops.c`, `util.c`, `slide.c` and
`root.c` — which do the actual writing — kept `target.h`'s compile-time constants.
The visible symptom was a non-page-aligned KASLR base: a run using the Ace 6T
fallback reported `open=…d854b4` on a device whose table said `d7ec68`, with every
delta exactly equal to the difference between the two constants.

`runtime_struct_offsets.h` is now pulled in at the end of `common.h` and rebinds
every one of those macros for all translation units. Only the entry that has been
tested against hardware is kept here.

### Not Feasible

The pselect stack overlay requires the freed `rt_mutex_waiter` to land within the user-controllable `stack_fds` region (words 0–14). Where it lands is determined by compiler PGO/LTO profiles, not the kernel version. See [Stack Layout](#stack-layout-feasibility).

| Device | SoC | Kernel | Root Cause |
|--------|-----|--------|------------|
| OPPO Find X9 Ultra | SM8750 | 6.12.58 | PGO eliminates `do_futex` → waiter word=14 |
| OnePlus 15 (PLK110) | SM8850 | 6.12.69 | PGO inlines `do_futex` → waiter word=20 |
| OnePlus 12 | SM8650 | 6.1.141 | PGO inlines `do_futex` → waiter word=13/19 |
| OnePlus 13R / Ace 5 | SM8650 | 6.1.x | OPLUS 6.1: waiter word=13 |
| realme RMX5070 | SM6650 | 6.1.141 | OPLUS 6.1: waiter word=13 |
| OPPO Pad 5 (OPD2502) | MT6878 | 6.1.134 | OPLUS 6.1: waiter word=13 |
| Motorola Edge 60 Fusion | MT6878 | 6.1.145 | Non-OPLUS 6.1: same result |
| iQOO Neo 10 CN | SM8650 | 6.1.84 | `do_futex` frame 0xD0 → waiter word=-11 |
| OPPO PKW110 | — | 5.15.180 | `do_futex` frame 0x140 → waiter word=-29 |
| iQOO Z9 5G | — | 5.15.178 | `do_futex` frame too large |
| CPH2763 (OPPO) | — | 6.1.115 | OPLUS 6.1: PGO inlined, waiter word=24 |
| iQOO 12 | SM8650 | 6.1.145 | 6.1: waiter word=9, task/lock in zeroed area |

> **Note on 6.1 feasibility:** OPLUS 6.1 kernels are consistently infeasible due to PGO inlining `do_futex`. However, some non-OPLUS 6.1 kernels (vivo) retain the standard call chain and are feasible (vivo T4, X Fold3 Pro). Feasibility must be checked per-device.

## OPPO Find X9 Pro (PLG110) — measured results

This fork targets a single device and replaces the upstream multi-device dispatch
with values that were confirmed on hardware
(`6.12.23-android16-5-g82efd98459a2-ab14457512-4k`, MT6993, Android 16, patch 2026-08-01).

**Call chain is feasible.** `__arm64_sys_futex` reaches `futex_wait_requeue_pi`
through `do_futex`, so the waiter lands at word 2 and `PSELECT_SHIFT=0` is correct.
`Path A (UMH)` is unavailable on this device: ashmem is the Rust implementation
(`ashmem_rust::ASHMEM_FOPS_PTR`), the miscdevice is heap-allocated, and
`off_ashmem_misc_fops` does not exist in the image. `Path B` is used.

**Measured writes.** `--write1` and the W2 cred write both succeed with
`pselect returned attempt=1 ret=4/5`; `ret=3` means the fd_set/waiter overlay
missed and the write will not land, so a failed attempt must be retried rather
than interpreted as an offset error. Observed result:

```
cfi attempt=1 fake_fops=ffffff80d7128100 target=ffffff80d7128380
    ioctl=ffffffc080d7e5a8 open=ffffffc080d7ec68 write_iter=ffffffc080512028
cfi write ret=-1 errno=22          # cosmetic: --write1 never uses the CFI stage
pselect route done calls=1 success=1 step=1 errno=22
SELinux DISABLED
```

```
round 1/10: cred write → child uid = 0 → child is root!
```

`errno=22` from the CFI stage is expected on this build and is not a failure:
`try_cfi_stage()` is only reached from the non-`--write1` branch, and the
`configfs_bin_write_iter` path is never used because Path A is unavailable.

**`kernel_phys_load=0x80000000`.** The bootloader places the image at physical
`0x80000000`; `/proc/iomem` reports `Kernel code` at `0x80010000` (`_stext`, with
`_text` 0x10000 lower). The offset entry leaves `.phys_offset` unset, which
`select_offsets()` reads as "keep `P0_PHYS_OFFSET`" — also `0x80000000` — so
`p0_data_alias()` works from a zero delta. Only the difference of the two values
is ever used, and it matches the image's `text_offset=0`.

**Two offsets in the table are zero on purpose.** `off_ashmem_misc_fops` and
`off_security_hook_heads` have no symbol on this kernel and are set to `0`.
`root.c` and `main.c` test them before use; do not "fix" them with a neighbouring
address, because `select_offsets()` performs no zero check on item offsets and a
value would silently turn into `KIMAGE_TEXT_BASE + 0`.

**Loading kernelsu.ko requires ksud.** `CONFIG_TRIM_UNUSED_KSYMS=y` means the
kernel exports only 8897 symbols, and the module needs 70 that are absent
(`path_umount`, `filp_open`, `commit_creds`, `kallsyms_on_each_symbol`, …).
A stock `insmod` fails with 181 unresolved symbols even though vermagic, module
signature and CRC versions all pass. `ksud insmod` resolves them from
`/proc/kallsyms` and is the only loader that works.

`allow_shell=1` must be passed on the **first** load. `ksud late-load` does not
forward module parameters, and `ksud unload` panics the kernel (the kallsyms
loader never completes the sysfs/RCU registration that `load_module()` performs).

## Exploit Flow

Two root paths. **This device only has Path B.**

### Path A: UMH Root (unavailable here)

Requires `off_ashmem_misc_fops != 0` (C ashmem with a static `miscdevice` in BSS).

```
PI write (mode=4)  →  redirect miscdevice fops to fake fops
                   →  configfs r/w → pipe physrw (1-byte precise kernel r/w)
                   →  SELinux enforcing = 0 (single byte, no policycap corruption)
                   →  UMH: inject work_struct into system_unbound_wq
                   →  root script → ksud insmod → KSU installed
```

6.12 GKI uses the Rust ashmem driver, whose `miscdevice` is heap allocated and has
no fops table in BSS, so `off_ashmem_misc_fops` is 0 and `main.c` takes Path B.
Note that `--write1` never enters `try_cfi_stage()`: the one-argument path writes
through the linear-map alias and needs no CFI pivot, which is why its
`cfi write ret=-1 errno=22` line is cosmetic.

### Path B: Direct PI Write

```
Write 1 (mode=1)  →  SELinux enforcing = 0 (8-byte write, corrupts adjacent bytes)
Write 2 (mode=2)  →  task->cred = init_cred (uid=0, all capabilities)
Root shell        →  ksud insmod → KSU → SELinux policy fix
```

After W1+W2 the exploit patches the SELinux policy binary's config field with
`|= 0xC0000000` (`ANDROID_NETLINK_ROUTE | ANDROID_NETLINK_GETNEIGH`) and reloads
it through `/sys/fs/selinux/load`.

Why both steps exist. W1 is an 8-byte zero write at `data_addr(SELINUX_ENFORCING)`,
and `struct selinux_state` starts with `bool enforcing` (+0x00), `bool initialized`
(+0x01) and `bool policycap[9]` (+0x02). The write therefore clears
`policycap[0..5]` — `netpeer_controls`, `open_perms`, `extended_socket_class`,
`always_check_network`, `cgroup_seclabel`, `nnp_nosuid_transition`. With
`always_check_network` cleared the kernel starts enforcing netif egress/ingress,
`netd` gets `tclass=netif` denials, and the device has no network. Reloading the
policy image makes the kernel re-derive those capabilities.

Measured on PLG110 after the repair, with `enforce=1`:

```
[*] policy fix: size=2506021 id_len=8 config_off=20 bytes=1 0 0 0
[*] policy fix: want config bytes=010000c0
[*] policy fix: patched config readback=010000c0
[*] policy fix: load_policy rc=0
[+] policy fix: policycaps re-derived from the policy image
```

`ping 192.168.3.1`, `ping 223.5.5.5` and DNS resolution all succeed, and
`tclass=netif` denials stop growing (11 before, 11 after fifteen seconds of
traffic).

### The bug that made this repair fail

The upstream script wrote its four bytes with
`printf '\x%02x\x%02x\x%02x\x%02x'`. toybox rejects `\x` in a format string:

```
$ printf '\x%02x\x%02x\n' 1 1 | od -A n -t x1
 70 72 69 6e 74 66 3a 20 62 61 64 20 5c 78 0a     # "printf: bad \x"
```

It emits the literal bytes `5c 78`, corrupting the policy header, so
`security_load_policy()` returned EINVAL on every attempt — eight out of eight in
`.ghostlock_policy.log`. `printf` only expands escapes in the **format** string,
never in `%s` arguments, so the working form builds the escape text with `%03o`
and then feeds it to `printf`:

```sh
ESC=$(printf '\\%03o\\%03o\\%03o\\%03o' $B0 $B1 $B2 $H3)
printf "$ESC" | dd of=$T bs=1 seek=$CO conv=notrunc
```

`|= 0xC0000000` only sets bits 30 and 31, i.e. the top byte, so the script ORs
just that byte (`B3 | 0xC0`) and carries the other three through untouched. Every
comparison is a byte string from `od -t x1` — toybox arithmetic is signed 32-bit
and prints `$((1 | 0xC0000000))` as `-1073741823`, so numeric comparison and
shifting are avoided entirely. If the patched load still fails, the script retries
with the pristine policy: reloading the running policy alone restores the caps.

### Bootstrap Mode (phone standalone)

```
App (seccomp)  →  Write 1 → mini-adb TCP → adb shell: full exploit → root
```

### Auto-Boot

There is no persistence path that works on this device. Everything below was
checked, and each one fails:

- **Boot image patch** — needs an unlocked bootloader. OPPO ships it locked and
  `init_boot.img` is not writable even with temporary root.
- **KernelSU module / metamodule** — requires an already-persistent KernelSU,
  which is the thing being installed.
- **`/data/adb/late-load.d`** — `ksud` does not read that directory. The string
  does not occur in the binary; it only reads `/data/adb/ksu`,
  `/data/adb/modules`, `/data/adb/modules_update`, `/data/adb/post-fs-data.d`
  and `/data/adb/service.d`. The [Anchor app](https://github.com/byemaxx/ghostlock-anchor)
  writes `/data/adb/late-load.d/99-anchor-recover.sh`, so that script never runs.
  Anchor's own end-condition is `waitForRoot()` inside the current process, not a
  check after reboot.
- **`/system/bin/su`** — a path the KernelSU execve hook fabricates. `stat`
  reports a plausible size, but the file is not in the `/system` listing and `/`
  is a read-only erofs. It cannot be copied aside and cannot survive a reboot.

Reaching root again after a reboot means re-running this exploit. On this device
that requires USB debugging to still be reachable, which is why the root script
also sets `persist.adb.tcp.port` when it gets that far.

## Stack Layout Feasibility

With `NFDS=320`, `core_sys_select` allocates a 256-byte `stack_fds` buffer on the kernel stack:

```
stack_fds:  0    5    10   14 | 15   20   25   29
            ├─in─┤─out─┤─ex──┤ ├res_in┤res_out┤res_ex┤
            ◄── USER CONTROLLED ──►│◄── KERNEL ZEROED ──►
```

The exploit places fake waiter fields (task, lock) in the fd_set input bitmaps. For 6.12 nested waiter (14 words): max feasible waiter word = **3**. For 5.10/6.1 compact waiter (10 words): max feasible waiter word = **7**.

The waiter position depends on the call chain depth:

| Pattern | Call Chain | Feasible |
|---------|-----------|----------|
| android16-5 (6.12) | sys_futex → do_futex → fwrpi | ✅ waiter word=2 |
| android16-6 (6.12.58) | sys_futex → do_futex → fwrpi | ✅ waiter word=2 |
| android16-6 (6.12.69 OPLUS) | sys_futex → fwrpi (PGO inlined) | ❌ waiter word=20 |
| android16-6 (X9 Ultra) | sys_futex → fwrpi (PGO inlined) | ❌ waiter word=14 |
| android15-8 (6.6) | sys_futex → do_futex → fwrpi | ✅ waiter word=2 (SHIFT=-2) |
| vivo 6.1 | sys_futex → do_futex → fwrpi | ✅ waiter word=3 |
| OPLUS 6.1 | sys_futex → fwrpi (PGO inlined) | ❌ waiter word=13+ |
| 5.10 OPLUS | sys_futex → do_futex → fwrpi | ✅ waiter word=0 |

### kernel_phys_load

All kernel writes go through the linear-map alias
(`p0_data_alias()`, `util.c:292`), which uses only the *difference* between
`kernel_phys_load` and `phys_offset`:

```c
uint64_t phys = p0_kernel_phys_load + (image_addr - KIMAGE_TEXT_BASE);
return ((phys - p0_phys_offset) | P0_PAGE_OFFSET);
```

`select_offsets()` copies a table entry's `kernel_phys_load` and `phys_offset`
into the globals only when the entry's value is non-zero, otherwise `target.h`
supplies it. On this device both resolve to `0x80000000`, so the delta is 0.

| SoC | kernel_phys_load |
|-----|------------------|
| MT6993 (Find X9 Pro) | `0x80000000` |
| SM8845 (Ace 6T, 15T) | `0xa8000000` |
| SM8750 (OnePlus 13, OPPO Pad 4 Pro) | `0xa8000000` |
| SM8650 (vivo T4, X Fold3 Pro) | `0xa8000000` |
| SM8850 (OnePlus 15, Xiaomi 17) | `0xc7800000` |

A wrong delta fails silently — writes land in unrelated RAM with no fault. Read
it on a rooted unit and remember `_stext` sits 0x10000 above `_text`:

```bash
su -c 'grep -i "Kernel code" /proc/iomem'   # c7810000-... → 0xc7800000
```

`KPHYS=...` overrides `p0_kernel_phys_load` at runtime (`util.c:282`). It does
not touch `p0_phys_offset`, so the override sets the delta directly.

### PSELECT_SHIFT

```bash
/data/local/tmp/a/e                        # Default (shift=0)
PSELECT_SHIFT=-2 /data/local/tmp/a/e       # OnePlus 13 (6.6)
```

### KIMAGE_TEXT_BASE

5.10 kernels use `0xffffffc008000000` (VA_BITS=39, different from 6.x default `0xffffffc080000000`). This is handled automatically via the `kimage_text_base` field in the device offset entry.

## Build

```bash
make NDK_ROOT=/path/to/android-ndk
```

On Windows use the bundled script instead — the NDK ships clang only as a
`.cmd` wrapper, which drops quoted `-D` values:

```powershell
$env:NDK_ROOT = "C:\android-ndk-r27d"
.\build.ps1
```

Both run the same checks afterwards: every translation unit must bind its
per-device offsets through `active_offsets`, and the embedded root script must
not call `ksud unload`.

Or directly:

```bash
NDK=/path/to/android-ndk
"$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang" \
  -O2 -Wall -Isrc/core -Isrc/devices \
  src/core/main.c src/core/util.c src/core/slide.c \
  src/core/fops.c src/core/pipe_physrw.c src/core/root.c \
  src/core/miniadb.c src/core/umh_root.c \
  -o ./ghostlock -fPIE -pie -pthread
```

`-DTARGET_CONFIG_H` is unnecessary: `offset.h` defaults it to `target.h`.

## Prerequisites

### ksud (required for KSU installation)

GhostLock provides temporary root. KernelSU installation requires **ksud**, which
loads `kernelsu.ko` through `/proc/kallsyms` relocation. This device's kernel is
built with `CONFIG_TRIM_UNUSED_KSYMS=y` and exports only 8897 symbols, so a stock
`insmod` cannot resolve 181 of the module's needs and exits 2.

Neither of the methods below supplies the module binary — the APK ships
`libksud.so`, a KMI **name table**, not the `.ko` bytes. Fetch the module for the
matching KMI separately (here `android16-6.12`):

```bash
adb push lkm-aarch64-android16-6.12_kernelsu.ko /data/local/tmp/a/kernelsu.ko
adb push  ksud-aarch64-linux-android           /data/local/tmp/a/ksud
```

| Method | Steps |
|--------|-------|
| **ReSukiSU APK** (recommended) | Install [ReSukiSU](https://github.com/ReSukiSU/ReSukiSU) or this [fork](https://github.com/JoinChang/ReSukiSU). Bundles `libksud.so`. |
| **CI release** | Download from [ReSukiSU CI](https://github.com/cctv18/ReSukiSU_CI/releases) |

The kernel hardcodes `me.weishu.kernelsu` as the manager package, so a ReSukiSU
APK is never recognised as the manager. Install the official
[KernelSU manager](https://github.com/tiann/KernelSU/releases) to get a working UI.

## Setup

```bash
adb push ./ghostlock    /data/local/tmp/a/e
adb push ./kernelsu.ko  /data/local/tmp/a/kernelsu.ko
adb push ./ksud         /data/local/tmp/a/ksud
adb shell chmod 755 /data/local/tmp/a/e /data/local/tmp/a/ksud
adb shell /data/local/tmp/a/e --write1     # SELinux permissive only, lowest risk
adb shell /data/local/tmp/a/e              # full run: W1 + W2 + KSU install
```

Run after boot completes and ADB is ready.

**Retry policy.** The overlay is timing dependent. If a run reports
`pselect returned ... ret=3` and the write does not land, run it again; do not
change offsets. A `ret=4` or `ret=5` on the first attempt is the normal success
path. On this device roughly one run in five reaches the PI chain with a NULL
`waiter->task`, which panics the kernel and reboots; the device recovers on its
own and the exploit can simply be re-run.

**`--write1` skips KASLR.** It sets `kaslr_slide=0` because W1 writes through the
linear-map alias and needs no slide. Do not use `--write1` to run W2.

## Adding New Devices

Only `boot.img` is needed — no root, no device access required.

### 1. Extract kernel and kallsyms

See [kallsyms tools guide](tools/kallsyms/GUIDE.md) for the full workflow, or:

```bash
python tools/extract_target.py --kallsyms kallsyms.txt
python tools/extract_btf.py kernel
```

### 2. Determine feasibility

Check if `do_futex` is PGO-inlined by searching the kernel binary for all branch instructions targeting `futex_wait_requeue_pi`. If `__arm64_sys_futex` calls it directly (bypassing `do_futex`), the device is likely not feasible.

### 3. Create device entry

Add `src/devices/<name>/offsets.h` with the extracted offsets and include it in
`src/devices/offsets.h`. Use the appropriate struct offsets macro:

| Kernel | Waiter Layout | Macro |
|--------|--------------|-------|
| 6.12.x | Nested (14 words) | `STRUCT_OFFSETS_6_12` |
| 6.6.x | Nested (14 words) | `STRUCT_OFFSETS_6_6` |
| 6.1.x (vivo) | Compact (10 words) | `STRUCT_OFFSETS_6_1` |
| 5.10.x | Compact (10 words) | `STRUCT_OFFSETS_5_10` |

For 5.10/6.1 compact waiter devices, also set `.kimage_text_base=0xffffffc008000000ULL`.

Fields where `0` is a real answer — `off_ashmem_misc_fops`, `off_security_hook_heads`
— go through `_RSO_RAW()` and are passed through as-is. Every other field falls
back to `target.h` when the entry has `0`, so a partially filled entry silently
borrows the primary device's offsets. Fill in all of them.

### 4. Verify on the device

Static offsets are not enough. Confirm, in this order:

1. `uname -r` matches the entry exactly — `select_offsets()` does a `strcmp`.
2. The route lands: `pselect returned ... ret=4/5`. `ret=3` means the fd_set
   layout missed; try `PSELECT_SHIFT` before suspecting offsets.
3. `pselect route done` prints the ashmem function addresses it read from the
   fops table. They must equal the table's `off_ashmem_*` plus the KASLR base; a
   constant delta means a translation unit is still using a compile-time value.
4. `--write1` flips `/sys/fs/selinux/enforce` to `0`.
5. The W2 round reports `child uid = 0`.

`build.ps1` / `make check` assert (3) at build time by preprocessing each
translation unit and requiring an `active_offsets->off_` reference.

## Files

| File | Description |
|------|-------------|
| `src/core/main.c` | Exploit entry, W1/W2, UMH path, bootstrap, root script, SELinux policy fix |
| `src/core/fops.c` | pselect route, PI write, CFI stage, compact waiter support |
| `src/core/util.c` | Heap spray, KernelSnitch, slab drain, payload setup |
| `src/core/slide.c` | KASLR leak through the PI tree anchors |
| `src/core/pipe_physrw.c` | Pipe buffer physical memory r/w |
| `src/core/umh_root.c` | UMH root via workqueue injection |
| `src/core/root.c` | Child-process root, cred/SELinux blob patching |
| `src/core/miniadb.c` | Mini ADB client (TCP + RSA auth) |
| `src/core/target.h` | Memory layout and compile-time fallbacks (PLG110) |
| `src/core/runtime_struct_offsets.h` | Rebinds every per-device offset to `active_offsets` for all TUs |
| `src/devices/offsets.h` | `struct kernel_offsets`, `STRUCT_OFFSETS_*`, device table |
| `src/devices/findx9pro/offsets.h` | The verified PLG110 entry |
| `tools/extract_target.py` | Offset extraction from kallsyms |
| `tools/extract_btf.py` | Struct offset extraction from BTF |
| `tools/kallsyms/` | End-to-end kallsyms recovery workflow |
| `build.ps1` | Windows build + the offset/`unload` assertions |

`src/core/pipe.c` is not part of the build. It is an unfinished alternative pipe
implementation that exports the same twelve symbols as `pipe_physrw.c`; adding it
to `SRCS` fails to link.

## License

For authorized security research and educational purposes only.
