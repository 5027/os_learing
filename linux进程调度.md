```text
#include <unistd.h>
#include <sched.h>
#include <errno.h>

int main(){
    struct sched_param priority,call;
    priority.sched_priority = 10;
    
     struct rlimit rlim;
    // 打印当前进程的优先级
    printf("Before: %d\n", getpriority(PRIO_PROCESS, getpid()));

    // 设置调度策略为 SCHED_RR，并设置优先级
    if (sched_setscheduler(0, SCHED_FIFO, &priority)<0) {
        perror("sched_setscheduler");
        return 1;
    }
     if (getrlimit(RLIMIT_RTPRIO, &rlim) == -1) {
        perror("getrlimit failed");
        return 1;
    }

    // 打印 RLIMIT_RTPRIO 的值
    printf("RLIMIT_RTPRIO: soft limit = %ld, hard limit = %ld\n", rlim.rlim_cur, rlim.rlim_max);
    //查优先级
    sched_getparam(0,&call);
    // 打印修改后的优先级
    printf("After: %d,sched_fifo:%d\n",call.sched_priority,SCHED_FIFO);
    printf("%d\n",getpriority(PRIO_PROCESS, 0));
    return 0;
}
```

这段代码我当时写出来之后一直在想，为什么getpriority的返回值永远是0呢，我不是已经通过sched_setscheduler 去设置了调度优先级吗。

然而我用sched_getparam查询了又发现我这个值不是已经变更为10了吗？

上网一搜才知道在进程的调度策略不同，有一套不同的优先权。

**非实时进程**（`SCHED_OTHER`），一般更根据nice值来进行调度。而实时进程优先级由 **显式设置的优先级值** 决定，而**`nice` 值对实时进程没有影响，**实时进程比如sched_fifo,sched_rr。

实时调度又分为软实时和硬实时。

linux并没由提供硬实时，硬实时就是类似于自动驾驶系统等待，对任务有着严格的时间要求

软实时则是可以接受一点误差，视频流和多媒体播放，电子游戏都是软实时。