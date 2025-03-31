root@qemuarm64:~# echo “hello_world” > /dev/faulty
[  892.149884] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
[  892.150791] Mem abort info:
[  892.150926]   ESR = 0x0000000096000045
[  892.151216]   EC = 0x25: DABT (current EL), IL = 32 bits
[  892.151554]   SET = 0, FnV = 0
[  892.151972]   EA = 0, S1PTW = 0
[  892.152185]   FSC = 0x05: level 1 translation fault
[  892.152515] Data abort info:
[  892.152903]   ISV = 0, ISS = 0x00000045
[  892.153166]   CM = 0, WnR = 1
[  892.153638] user pgtable: 4k pages, 39-bit VAs, pgdp=00000000437c3000
[  892.155067] [0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
[  892.157666] Internal error: Oops: 0000000096000045 [#1] PREEMPT SMP
[  892.159986] Modules linked in: scull(O) faulty(O) hello(O)
[  892.165230] CPU: 3 PID: 410 Comm: sh Tainted: G           O      5.15.178-yocto-standard #1
[  892.166074] Hardware name: linux,dummy-virt (DT)
[  892.166645] pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[  892.166836] pc : faulty_write+0x18/0x20 [faulty]
[  892.167452] lr : vfs_write+0xf8/0x2a0
[  892.167552] sp : ffffffc009743d80
[  892.167621] x29: ffffffc009743d80 x28: ffffff8003df6040 x27: 0000000000000000
[  892.167870] x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
[  892.168007] x23: 0000000000000000 x22: ffffffc009743dc0 x21: 000000558eb18cf0
[  892.168137] x20: ffffff80036bf200 x19: 0000000000000012 x18: 0000000000000000
[  892.168266] x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
[  892.168396] x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
[  892.168527] x11: 0000000000000000 x10: 0000000000000000 x9 : ffffffc008271f58
[  892.168708] x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
[  892.168858] x5 : 0000000000000001 x4 : ffffffc000b95000 x3 : ffffffc009743dc0
[  892.168990] x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
[  892.169275] Call trace:
[  892.169367]  faulty_write+0x18/0x20 [faulty]
[  892.169490]  ksys_write+0x74/0x110
[  892.169556]  __arm64_sys_write+0x24/0x30
[  892.169627]  invoke_syscall+0x5c/0x130
[  892.169729]  el0_svc_common.constprop.0+0x4c/0x100
[  892.169827]  do_el0_svc+0x4c/0xc0
[  892.169894]  el0_svc+0x28/0x80
[  892.169958]  el0t_64_sync_handler+0xa4/0x130
[  892.170032]  el0t_64_sync+0x1a0/0x1a4
[  892.170251] Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
[  892.170653] ---[ end trace 00b1c3805d75f37e ]---
Segmentation fault

Poky (Yocto Project Reference Distro) 4.0.25 qemuarm64 /dev/ttyAMA0

qemuarm64 login: 

