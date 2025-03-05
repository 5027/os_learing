# **rCore 文件系统（rcore-fs）**

## **1. 文件系统概述**

- 我个人感觉这一方面类似于Mysql中的持久化部分，很多可以参考mysql的优化来做

- 出于解耦合考虑，文件系统 easy-fs 被从内核中分离出来，分成两个不同的 crate ：

  - `easy-fs` 是简易文件系统的本体；
  - `easy-fs-fuse` 是能在开发环境（如 Ubuntu）中运行的应用程序，用于将应用打包为 easy-fs 格式的文件系统镜像，也可以用来对 `easy-fs` 进行测试。

  easy-fs与底层设备驱动之间通过抽象接口 `BlockDevice` 来连接，采用轮询方式访问 `virtio_blk` 虚拟磁盘设备，避免调用外设中断的相关内核函数。easy-fs 避免了直接访问进程相关的数据和函数，从而能独立于内核开发。

  `easy-fs` crate 以层次化思路设计，自下而上可以分成五个层次：

  1. 磁盘块设备接口层：以块为单位对磁盘块设备进行读写的 trait 接口
  2. 块缓存层：在内存中缓存磁盘块的数据，避免频繁读写磁盘
  3. 磁盘数据结构层：磁盘上的超级块、位图、索引节点、数据块、目录项等核心数据结构和相关处理
  4. 磁盘块管理器层：合并了上述核心数据结构和磁盘布局所形成的磁盘文件系统数据结构
  5. 索引节点层：管理索引节点，实现了文件创建/文件打开/文件读写等成员函数



##  磁盘布局与数据结构

**超级块 (Super Block)**
存储文件系统元信息，通过 `SuperBlock` 结构体定义：

```
pub struct SuperBlock {
    magic: u32,          // 文件系统标识 "rfs!"
    total_blocks: u32,   // 总块数
    inode_bitmap_blocks: u32,  // inode位图占用块数
    data_bitmap_blocks: u32,   // 数据位图占用块数
    inode_area_blocks: u32,    // inode表区域块数
    data_area_blocks: u32,     // 数据区域块数
    root_inode: u32,          // 根目录inode号
}
```

#### **磁盘 Inode 结构**

```
pub struct DiskInode {
    pub size: u32,               // 文件大小（字节）
    pub type_: DiskInodeType,    // 文件类型（File/Dir/SymLink）
    pub blocks: [u32; NDIRECT + 1], // 数据块指针
    // NDIRECT=27，最后一位存储间接块指针
}
```

- **数据块索引策略**：
  - **直接索引**：前 27 项直接指向数据块（最大文件大小 = 27 * BLOCK_SIZE）
  - **一级间接索引**：第 28 项指向一个间接块（额外增加 256 块索引，BLOCK_SIZE=512时）
  - **最大文件计算**：27*512B + 256*512B = 13824KB ≈ 13.5MB

#### **内存 Inode 结构**

```
pub struct Inode {
    block_id: usize,       // 所在块号
    block_offset: usize,  // 块内偏移
    fs: Arc<Mutex<FileSystem>>, // 所属文件系统
    block_device: Arc<dyn BlockDevice>, // 块设备接口
}
```

```
pub struct DirEntry {
    inode_number: u32,                // 对应inode编号
    name: [u8; NAME_LENGTH_LIMIT + 1], // 文件名（最大28字节）
}
```

- **目录文件内容**：由多个 `DirEntry` 连续组成，每个条目固定 32 字节
- **特殊目录项**：
  - `.` 表示当前目录（inode 相同）
  - `..` 表示父目录（根目录的父目录指向自己）

------

## 分层架构



| 层级         | 组件                | 职责说明                   |
| :----------- | :------------------ | :------------------------- |
| 块设备驱动层 | `BlockDevice` trait | 提供统一块读写接口         |
| 块缓存层     | `BlockCache`        | LRU缓存 + Dirty标志管理    |
| 索引管理层   | `Inode` 相关结构    | 文件/目录的元数据管理      |
| 文件接口层   | `File` trait        | 实现读写接口供系统调用使用 |

### 块缓存机制

**缓存结构**：

```
pub struct BlockCache {
    cache: Vec<u8>,          // 块数据（长度=BLOCK_SIZE）
    block_id: usize,         // 块号
    modified: bool,          // 脏标记
}
```

同步策略**：

- 读取时：优先从缓存获取，未命中则加载到缓存
- 写入时：修改缓存并标记为脏，通过 `sync` 主动刷盘

### ** 文件创建**

1. **分配 inode**：

   - 扫描 inode 位图，寻找空闲位
   - 更新位图并初始化 `DiskInode`

2. **创建目录项**：

   ```
   // 在父目录中添加条目
   fn add_entry(&self, name: &str, new_inode: u32) -> Result<()> {
       let mut entries = self.read_dir()?;
       entries.push(DirEntry::new(name, new_inode));
       self.write_dir(&entries)
   }
   ```

   **元数据同步**：

   - 更新父目录的 inode 大小
   - 刷新相关块缓存

### **文件查找实现**

```
impl Inode {
    pub fn find(&self, name: &str) -> Result<Arc<Inode>> {
        let entries = self.read_disk_inode(|disk_inode| {
            self.read_as_dir(disk_inode)
        })?;
        for entry in entries {
            if entry.name() == name {
                return self.fs.get_inode(entry.inode_number());
            }
        }
        Err(FsError::EntryNotFound)
    }
}
```



### ** 文件写入流程**

```
impl File for Inode {
    fn write(&self, mut offset: usize, buf: &[u8]) -> Result<usize> {
        let mut disk_inode = self.fs.lock().get_disk_inode(self.id);
        // 计算需要扩展的块数
        let total_blocks = (offset + buf.len() + BLOCK_SIZE - 1) / BLOCK_SIZE;
        self.alloc_blocks(total_blocks)?;
        // 逐块写入数据
        for chunk in buf.chunks(BLOCK_SIZE) {
            let block_id = self.get_block_id(offset / BLOCK_SIZE);
            let cache = self.block_cache(block_id);
            cache.modify(offset % BLOCK_SIZE, chunk);
            offset += chunk.len();
        }
        disk_inode.size = offset as u32;
        Ok(buf.len())
    }
}
```

1. 检查是否需要分配新块
2. 通过 `get_block_id` 查找物理块号
3. 使用块缓存进行数据写入
4. 更新文件大小