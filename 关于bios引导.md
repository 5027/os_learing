本贴用来记录作者用c语言写一个操作系统，主要参考《操作系统真相还原》一书写的，同时也会对书里的代码和linux进行对比，尽量看一下现代操作系统中是如何实现的。

传统的操作系统课一般从内存，虚拟化等等方面讲起，因为是自己实现操作系统，肯定不能一上来就写开始写内存管理这种大活，因此我们从操作系统的启动开始说起。

重装过系统的都知道，操作系统的启动之前都会有一个BIOS界面，因为内存断电之后会被清空，所以就要重新加载操作系统到内存里，这个职责的第一棒就是BIOS。BIOS存在一个类似于硬盘的芯片中，即使断电了也不会丢失。

一上电瞬间加载的第一条指令地址CS:IP指向0xF000:0xFFF0 = 0xFFFF0，也就是1MB的最高16B处，这条指令就是BIOS的位置。当然16B肯定啥东西都放不了，所以这里只是一个跳转指令的地址，他会跳转到别的位置来执行。

![img](https://pica.zhimg.com/80/v2-a8501427ef230f38bed1723d84b20500_1440w.webp?source=d16d100b)

开机的时候1mb大小的内存都是固定分配好的

之后CPU会通过jmp far f000:e05b也就是0xfe05b处开始执行真正的BIO代码，然后BIOS就开始马不停蹄的检查内存，显卡之类的硬件信息，同时建立好相关的数据结构，之后BIOS的使命也就完成了，他要把重任交给下一个程序。

接下来电脑会开始检查硬盘，如果硬盘第一个扇区末尾的两个字节是0x55和0xaa，BIOS就会认为这个扇区存在可执行程序，然后把他加载到内存地址0x7c00处，开始执行，这个被加载的程序就是MBR，他接过了引导操作系统的重任。

MBR的大小是512字节，主要由以下部分组成：

- **MBR引导代码**：通常占用前446字节。
- **分区表**：占用接下来的64字节，用来描述硬盘上的分区信息。
- **MBR标志**：占用最后2字节。

![img](https://picx.zhimg.com/80/v2-c7b20e6d641ba662fbc18716e0a219cc_1440w.webp?source=d16d100b)

大家也没有看出来一个问题，MBR为啥这么小？因为早期受硬件限制一个扇区只有512字节，即使大多数现代硬盘依然在物理层面使用4K扇区，但为了兼容旧系统，它们通常会通过“512字节逻辑扇区”来模拟。而我们的MBR只能存在第一个扇区中，所以MBR充其量也只能充当一个跳转程序（就像BIOS那样），不能承担加载操作系统的重任，因此MBR的职责就是从硬盘里读取到真正的加载内核的程序。

MBR程序大概如下（当然这是书上的，不是linux的mbr）

```text
;功能:读取硬盘n个扇区
rd_disk_m_16:	   
;-------------------------------------------------------------------------------
				       ; eax=LBA扇区号
				       ; ebx=将数据写入的内存地址
				       ; ecx=读入的扇区数
      mov esi,eax	  ;备份eax
      mov di,cx		  ;备份cx
;读写硬盘:
;第1步：设置要读取的扇区数
      mov dx,0x1f2
      mov al,cl
      out dx,al            ;读取的扇区数

      mov eax,esi	   ;恢复ax

;第2步：将LBA地址存入0x1f3 ~ 0x1f6

      ;LBA地址7~0位写入端口0x1f3
      mov dx,0x1f3                       
      out dx,al                          

      ;LBA地址15~8位写入端口0x1f4
      mov cl,8
      shr eax,cl
      mov dx,0x1f4
      out dx,al

      ;LBA地址23~16位写入端口0x1f5
      shr eax,cl
      mov dx,0x1f5
      out dx,al

      shr eax,cl
      and al,0x0f	   ;lba第24~27位
      or al,0xe0	   ; 设置7～4位为1110,表示lba模式
      mov dx,0x1f6
      out dx,al

;第3步：向0x1f7端口写入读命令，0x20 
      mov dx,0x1f7
      mov al,0x20                        
      out dx,al

;第4步：检测硬盘状态
  .not_ready:
      ;同一端口，写时表示写入命令字，读时表示读入硬盘状态
      nop
      in al,dx
      and al,0x88	   ;第4位为1表示硬盘控制器已准备好数据传输，第7位为1表示硬盘忙
      cmp al,0x08
      jnz .not_ready	   ;若未准备好，继续等。

;第5步：从0x1f0端口读数据
      mov ax, di
      mov dx, 256
      mul dx
      mov cx, ax	   ; di为要读取的扇区数，一个扇区有512字节，每次读入一个字，
			   ; 共需di*512/2次，所以di*256
      mov dx, 0x1f0
  .go_on_read:
      in ax,dx
      mov [bx],ax
      add bx,2		  
      loop .go_on_read
      ret

   times 510-($-$$) db 0
   db 0x55,0xaa
```

MBR 引导过程发生在操作系统加载之前，此时操作系统的内存管理和驱动程序还未初始化。因此，MBR 无法依赖内存映射 I/O 来控制硬盘，而必须通过 I/O 端口进行低级操作，MBR正是通过端口把硬盘里的数据加载到内存中，之后再通过一个指令，跳转到刚刚从硬盘里加载好的程序处，也就是我们常说的loader。

```text
jmp LOADER_BASE_ADDR + 0x300
```

如果想要查看linux的MBR程序，可以通过parted -l用于列出你所有的硬盘信息，输出大概是这样

```text
$ sudo parted -l
Model: ATA TOSHIBA THNSNS25 (scsi)
Disk /dev/sda: 256GB
Sector size (logical/physical): 512B/512B
Partition Table: msdos

Number  Start   End     Size    Type     File system  Flags
 1      4194kB  32.2GB  32.2GB  primary  ext4         boot
 2      32.2GB  256GB   224GB   primary  ext4
```

我们找到其中的Partition Table一项，如果看到是msdos话就说明这个硬盘里有MBR，为什么分区表一项（Partition table）就代表用mbr引导呢？因为MBR的概念就是MS-DOS（一个很古老的操作系统）在1983年引入的。

然后通过

```text
dd if=/dev/sda of=mbr.bin bs=512 count=1
```

把他拷贝到你的文件夹下，然后反汇编，就可以阅读这一部分的代码了，奥对了，记得把sda替换成你刚刚指令上找到有MBR程序的硬盘（wsl的同学就不用试了，没有MBR，可以看看这个文章[SamuelHuang：基于Grub 2.00的x86内核引导流程--源代码情景分析（2）](https://zhuanlan.zhihu.com/p/28300233)）。

loader因为不像MBR一样受硬盘大小限制，所以终于可以承担起加载操作系统的重任，那么把加载一个操作系统要做到什么呢？

x86 处理器在上电后默认处于实模式（Real Mode），这是一种兼容早期处理器（如 8086）设计的模式。在实模式下，CPU 只有 20 位地址线，可以寻址最多 1MB 的内存，且没有现代处理器所提供的内存保护、虚拟内存和分页机制等特性，会搞的程序员非常头大，所以就要使用一个可以支持更多内存配置以及32位处理器的模式，保护模式因此应运而生。

我们简易版本的loader就要做到能加在保护模式，大概分为三步：

- **启用 A20 地址线：** A20 地址线是控制是否可以访问 1MB 以上内存的开关。在实模式下，A20 地址线通常被禁用，因此访问超过 1MB 的内存会导致访问错误。进入保护模式之前，必须启用 A20 地址线。
- **加载全局描述符表（GDT）：** 保护模式下，内存管理依赖于全局描述符表（GDT），这需要在进入保护模式之前正确设置。
- **设置控制寄存器（CR0）：** 进入保护模式还需要设置 CPU 的控制寄存器（如 CR0）的标志位，启用保护模式。

```text
 ;-----------------  打开A20  ----------------
   in al,0x92
   or al,0000_0010B
   out 0x92,al

   ;-----------------  加载GDT  ----------------
   lgdt [gdt_ptr]

   ;-----------------  cr0第0位置1  ----------------
   mov eax, cr0
   or eax, 0x00000001
   mov cr0, eax

   jmp dword SELECTOR_CODE:p_mode_start	     ; 刷新流水线，避免分支预测的影响,这种cpu优化策略，最怕jmp跳转，
					     ; 这将导致之前做的预测失效，从而起到了刷新的作用。
```

之后呢，就要开始加载保护模式对应的虚拟内存了

```text
;-------------   创建页目录及页表   ---------------
setup_page:
;先把页目录占用的空间逐字节清0
   mov ecx, 4096
   mov esi, 0
.clear_page_dir:
   mov byte [PAGE_DIR_TABLE_POS + esi], 0
   inc esi
   loop .clear_page_dir

;开始创建页目录项(PDE)
.create_pde:				     ; 创建Page Directory Entry
   mov eax, PAGE_DIR_TABLE_POS
   add eax, 0x1000 			     ; 此时eax为第一个页表的位置及属性
   mov ebx, eax				     ; 此处为ebx赋值，是为.create_pte做准备，ebx为基址。

;   下面将页目录项0和0xc00都存为第一个页表的地址，
;   一个页表可表示4MB内存,这样0xc03fffff以下的地址和0x003fffff以下的地址都指向相同的页表，
;   这是为将地址映射为内核地址做准备
   or eax, PG_US_U | PG_RW_W | PG_P	     ; 页目录项的属性RW和P位为1,US为1,表示用户属性,所有特权级别都可以访问.
   mov [PAGE_DIR_TABLE_POS + 0x0], eax       ; 第1个目录项,在页目录表中的第1个目录项写入第一个页表的位置(0x101000)及属性(7)
   mov [PAGE_DIR_TABLE_POS + 0xc00], eax     ; 一个页表项占用4字节,0xc00表示第768个页表占用的目录项,0xc00以上的目录项用于内核空间,
					     ; 也就是页表的0xc0000000~0xffffffff共计1G属于内核,0x0~0xbfffffff共计3G属于用户进程.
   sub eax, 0x1000
   mov [PAGE_DIR_TABLE_POS + 4092], eax	     ; 使最后一个目录项指向页目录表自己的地址

;下面创建页表项(PTE)
   mov ecx, 256				     ; 1M低端内存 / 每页大小4k = 256
   mov esi, 0
   mov edx, PG_US_U | PG_RW_W | PG_P	     ; 属性为7,US=1,RW=1,P=1
.create_pte:				     ; 创建Page Table Entry
   mov [ebx+esi*4],edx			     ; 此时的ebx已经在上面通过eax赋值为0x101000,也就是第一个页表的地址 
   add edx,4096      ; edx
   inc esi
   loop .create_pte

;创建内核其它页表的PDE
   mov eax, PAGE_DIR_TABLE_POS
   add eax, 0x2000 		     ; 此时eax为第二个页表的位置
   or eax, PG_US_U | PG_RW_W | PG_P  ; 页目录项的属性US,RW和P位都为1
   mov ebx, PAGE_DIR_TABLE_POS
   mov ecx, 254			     ; 范围为第769~1022的所有目录项数量
   mov esi, 769
.create_kernel_pde:
   mov [ebx+esi*4], eax
   inc esi
   add eax, 0x1000
   loop .create_kernel_pde
   ret
```

之后，我们打开虚拟机就可以看到他们的映射关系

![img](https://pica.zhimg.com/80/v2-ba8f4f949ff46b8efceaf693a6d1e3d6_1440w.webp?source=d16d100b)

接下来就是读取内核数据
使用 `rd_disk_m_32` 函数通过 I/O 端口从硬盘读取内核镜像数据，
将内核镜像中的各个段复制到内存中，调用 `mem_cpy` 函数实现内存拷贝，
完成内核初始化后，跳转到内核入口点，开始执行操作系统内核，这就是《操作系统真相还原》中一个小的loader，这部分比较简单也不是很重要，我觉得就没必要看代码了，毕竟汇编是真的头痛啊。



而linux 下的loader我们一般用GRUB，他是这么启动的

1. **加载 GRUB 阶段 ：** 从mbr加载完之后，就是完整的 GRUB 引导程序，通常位于磁盘上的某个分区（如 /boot/grub）。在这个阶段，GRUB 会展示操作系统选择菜单，并读取配置文件。
2. **显示启动菜单：** GRUB 会展示一个菜单，用户可以选择要启动的操作系统（如 Linux 内核）。此时，GRUB 会加载内核映像文件（如 `vmlinuz`），并将控制权交给操作系统内核。
3. **传递内核参数：** GRUB 可以向操作系统内核传递启动参数，这些参数影响内核的行为（如指定根文件系统、禁用某些硬件等）。

如果你用的不是wsl，打开/boot/grub就可以看到他们，grub-mkconfig -o /boot/grub/grub.cfg会列出grub可以加载的操作系统（这玩意还能加载Windows）。

一种可能的引导加载程序方法是通过直接访问硬盘扇区来加载内核镜像，而无需理解底层的文件系统。通常，这需要一个额外的间接层，以映射或映射文件的形式——这些辅助文件包含了内核镜像所占据的物理扇区列表。

每次内核镜像在磁盘上更改物理位置时，如安装新内核镜像、文件系统碎片整理等，都需要更新这些映射文件。而且，如果映射文件的物理位置发生变化，它们的位置也需要在引导加载程序的 MBR 代码中进行更新，以使扇区间接机制继续有效。这不仅繁琐，而且在系统更新过程中出现问题时，仍然需要手动修复。

另一种方法是让引导加载程序了解底层的文件系统，这样内核镜像可以通过实际的文件路径进行配置和访问。这要求引导加载程序包含每个支持的文件系统的驱动程序，以便引导加载程序能够理解并访问这些文件系统。这种方法消除了硬编码硬盘扇区位置和映射文件的存在，也不需要在添加或移动内核镜像时更新 MBR 配置。引导加载程序的配置存储在一个常规文件中，该文件也是通过文件系统感知的方式进行访问，以便在实际引导任何内核镜像之前获取引导配置信息。因此，在系统更新过程中出现问题的可能性较小。缺点是，这种引导加载程序更大且更复杂。

我们的loader就是第一种，而GRUB是第二种。