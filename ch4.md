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
