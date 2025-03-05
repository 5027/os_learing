#### 如何获取物理地址

首先把usize转换为virtaddr，这样的话能屏蔽高位。

虚拟地址和物理地址偏移量相同再算出偏移量（根据viraddr）

然后再用floor获取当前的虚拟页号

获取虚拟页号之后再通过find_pte方法去查找虚拟页号对应的物理页号。

最后把物理页号加上虚拟页号就是地址

##### find_pte如何实现

###### 首先先要实现一个index方法

​	这个index方法会返回vpn的三级索引

然后再进行一次遍历，通过遍历来查找3级索引对应最后对应的物理地址39 位虚拟地址被分为三部分：

- **1 位**：保护位（通常用于标识特权级或有效性）。

- **10 位**：页目录索引（指向第一级页表）。

- **10 位**：页表索引（指向第二级页表）。

- **12 位**：页内偏移（指向页内具体地址）。

  

  

#### 如何实现mmap和munmap

我们先来谈谈mmap，首先先检查start和port的合法性，由于按页帧来分配内存，所以start一定是要被page——size整除的。核心是 `current_task.memory_set.insert_framed_area(start_address, end_address, permissions);`

这个permission通过对port进行位运算来获得，unmap出了最后是用free_framed_area其余的都一致。底层实现就是逐个解除page_table中的映射关系。

Sv39规范采用三级页表结构管理虚拟地址空间，支持最大512GB寻址能力。虚拟地址由39位有效位组成，其高25位需遵循符号扩展规则（63-39位与第38位保持相同值），将逻辑地址空间划分为高256GB内核区和低256GB用户区。这种设计有效隔离系统内核与用户程序，同时兼容64位架构的寻址需求。

三级页表采用分阶索引机制：

- 一级页表（页全局目录）：单条目映射1GB空间
- 二级页表（页中间目录）：单条目映射2MB空间
- 三级页表（页表项）：单条目管理4KB物理页

### 物理地址转换机制

物理地址采用56位编码，页表条目[53:10]位存储44位物理页号。转换过程遵循右移2位的对齐规则，确保页表项能准确索引物理页帧。条目末10位为控制标志位，其中Valid位（最低位）决定条目有效性，RWX权限组合控制访问模式。

#### 智能页表优化

支持混合粒度映射是大页技术的核心优势：

- 当非末级页表条目RWX非全零时，直接映射为大页（1GB/2MB）
- 大页要求物理地址按自身尺寸对齐
- 减少页表层级遍历次数，提升TLB命中率

例如内核启动阶段采用1GB大页映射关键区域，既可降低内存占用，又能提升地址转换效率。

### 内核地址重定位

内核镜像采用高位虚拟地址布局（0xffffffff80200000），通过地址偏移量实现虚实映射：

1. 物理地址0x80200000对应内核入口
2. 虚拟地址增加0x7f00000000偏移量
3. 建立双重映射保障分页启用后的平滑过渡

内存管理模块通过KERNEL_MAP_OFFSET常量实现虚实地址转换，关键数据结构包括：

```
struct VirtualAddress(usize);

impl VirtualAddress {
    // 虚实地址转换方法
    fn to_physical(&self) -> PhysicalAddress {
        PhysicalAddress(self.0 - KERNEL_MAP_OFFSET)
    }
}
```

### 页表管理实现

#### 页表条目封装

```
bitflags! {
    pub struct PageTableFlags: u8 {
        const VALID = 1 << 0;
        const READABLE = 1 << 1;
        const WRITABLE = 1 << 2;
        const EXECUTABLE = 1 << 3;
        const USER = 1 << 4;
        const GLOBAL = 1 << 5;
        const ACCESSED = 1 << 6;
        const DIRTY = 1 << 7;
    }
}

pub struct PageTableEntry {
    entry: AtomicU64,
}

impl PageTableEntry {
    // 设置物理页帧号与标志位
    pub fn set(&mut self, ppn: PhysPageNum, flags: PageTableFlags) {
        let value = (ppn.0 as u64) << 10 | flags.bits() as u64;
        self.entry.store(value, Ordering::Release);
    }
}
```

#### 多级页表操作

内存管理单元通过递归查询实现地址转换：

1. 从satp寄存器获取根页表物理地址
2. 逐级解析虚拟地址中的VPN字段（9位/级）
3. 动态创建缺失的页表结构
4. 末级条目完成物理页映射

```
impl MemoryMapper {
    pub fn map(&mut self, vpn: VirtPageNum, ppn: PhysPageNum, flags: PageTableFlags) {
        let entry = self.find_or_create_entry(vpn);
        entry.set(ppn, flags);
    }

    fn find_or_create_entry(&mut self, vpn: VirtPageNum) -> &mut PageTableEntry {
        // 三级页表遍历逻辑
        let idx1 = vpn.level_index(0);
        let idx2 = vpn.level_index(1);
        let idx3 = vpn.level_index(2);
        
        let l1 = self.root_table[idx1].get_or_alloc();
        let l2 = l1.table[idx2].get_or_alloc();
        &mut l2.table[idx3]
    }
}
```

### 内核启动优化

启动阶段采用混合映射策略保障平稳过渡：

1. 临时页表建立1GB大页双重映射
   - 物理地址0x80000000 -> 虚拟地址0x80000000
   - 物理地址0x80000000 -> 虚拟地址0xffffffff80000000
2. 配置satp寄存器启用分页模式
3. 执行sfence.vma指令刷新TLB
4. 跳转到高位虚拟地址继续执行

汇编关键代码示例：

```
# 配置临时页表
la t0, early_pgtable
li t1, (0x80000 << 10) | 0xCF  # 1GB大页，RWX权限
sd t1, (t0)

# 启用分页
li t2, SATP_MODE_SV39
srl t0, t0, 12
or t0, t0, t2
csrw satp, t0

# 跳转至高地址
la ra, high_mem_entry
jr ra
```

### 内存管理单元封装

系统通过分层抽象实现灵活的内存管理：

#### 虚拟内存区域（Segment）

```
pub enum SegmentType {
    Linear,    // 线性映射（设备内存）
    Framed,    // 动态分配（用户内存）
}

pub struct Segment {
    vpn_range: VPNRange,
    data_frames: BTreeMap<VirtPageNum, FrameTracker>,
    type_: SegmentType,
}
```

#### 地址空间管理（MemorySet）

```
pub struct MemorySet {
    mapping: Mapping,
    segments: Vec<Segment>,
    allocated_pairs: Vec<(VirtPageNum, FrameTracker)>,
}

impl MemorySet {
    pub fn new_kernel() -> Self {
        let mut ms = Self::empty();
        // 映射代码段
        ms.map_segment(Segment::new_linear(
            0x80200000..0x80400000,
            RWX_PERMS,
        ));
        // 映射设备内存
        ms.map_segment(Segment::new_linear(
            DEVICE_START..DEVICE_END,
            DEVICE_PERMS,
        ));
        ms
    }
}
```。
