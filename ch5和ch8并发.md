目前的内核仍然是单核架构，只有一个内核栈，并且不支持中断嵌套。当前的中断处理方式会在用户态和内核态均切换  sp  和  sscratch ，这种方式在中断嵌套时会导致问题。因此，需要优化为仅在用户态切换栈，而在内核态保持原栈，以为后续支持中断嵌套奠定基础。

**进程与线程的区别**
线程是调度的基本单位，而进程则包含地址空间。同一进程内的多个线程共享地址空间，因此在高地址区域会映射多个栈供不同线程使用。在创建新线程或  Context  时，都需要关联到对应的  process  结构体。

线程切换的实现方式是将当前线程的  Context  保存在中断栈上，并将下一个线程的  Context  放置到栈顶，从而完成切换。

------

### **代码**

#### **中断相关 ( interrupt/context.rs )**

Context  结构体的定义保持不变，但增加了  Default  实现，并用全零初始化。

提供  get/set  栈指针及返回地址的函数，以便管理  Context 。

设计了能够按照调用规则写入参数的函数，并提供用于创建新  Context  的方法，使其能够执行特定函数，并关联到  process  结构体。

#### **进程管理 ( process  相关模块)**

process.rs

 process  结构体主要用于封装  MemorySet ，管理进程的地址空间。

提供  new_kernel  用于创建内核进程，以及  from_elf （后续实验才会实现）用于从 ELF 文件创建进程。

 alloc_page_range  实现动态内存分配，类似  mmap ，通过遍历地址空间寻找可用区域，并在  MemorySet  中添加映射。

- ** kernel_stack.rs **

  由于线程已经在高地址区分配了独立栈，因此内核栈仅用于处理中断，而不会维护函数调用关系。

  在切换线程时， Context  直接放置在栈顶，以模拟正常的中断返回。

- ** thread.rs **

  Thread结构体包括 id、栈地址范围、所属进程（封装在  Arc<RwLock<Process>> 内），以及  inner （包含  Context 和  sleeping状态）。

  prepare 函数用于激活线程的页表，并将  Context 放置在公共内核栈顶，以支持线程切换。

  new  负责创建新线程，包括分配独立栈、构建  Context ，并关联到  Process 结构体。

#### **调度器 ( processor.rs )**

主要封装调度相关操作，并维护进程状态转换。

新增 **高响应比优先（HRRN）** 调度算法，在  hrrn_scheduler.rs  中实现。

线程被封装为  HrrnThread ，增加  birth_time  和  service_count  字段。

HrrnScheduler  采用  LinkedList组织线程，并维护当前时间信息。

通过  max_by()  选择响应比最高的线程执行。

processor.rs  主要通过  park + prepare  机制实现线程切换，并在  timer_interrupt  中触发调度。

由于单核环境，调度器的作用范围局限于当前 CPU，因此  Processor  仅需管理一个  current_thread  变量。

#### **中断 ( interrupt.asm  &  interrupt.rs )**

调整 

```
interrupt.asm
```

 以支持线程切换：

先交换  sscratch ，再保存  sp ，确保  Context  正确存放在栈上。

__restore 既可作为函数调用，也可作为中断返回路径。

通过  mv sp, a0  使  __restore  既能接受  Context  作为参数，也能返回新的  Context  以供切换。

interrupt.rs  处理外部中断，并新增  wait_for_interrupt()  供  processor.rs调用。

 handle_interrupt  统一管理不同类型的中断，异常发生时调用  fault()  处理。

------

### **其他优化**

#### **线程的组织方式**

采用  hashbrown  维护  sleeping  线程，而调度所需的线程则交由调度算法管理。

Arc<RwLock<Process>>  用于线程间共享  Process ，并通过  clone  机制允许多个线程持有同一进程。

#### **同步机制 ( lock.rs )**

Lock  结构体包装  Mutex  并增加 **关中断** 机制，在获取锁时自动关闭中断，并在释放时恢复。

 Drop  实现了锁的自动释放。

 deref  &  deref_mut  允许  Lock  直接用作智能指针访问内部数据。

unsafe_get  提供非锁定访问方式（如  PROCESSOR::run() ），适用于不会返回的场景。

#### **线程结束机制**

线程终止时，设置  isDead  标志，在下一次中断时触发清理。

线程退出后，调度器会选择下一个可运行线程执行。