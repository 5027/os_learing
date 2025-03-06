### [141. 环形链表](https://leetcode.cn/problems/linked-list-cycle/)

这一题龟兔赛跑法

 if(fast == slow) return true; 这个判断比较合理，因为可能出现相同的值但是指向的地址是不一样的

### [2. 两数相加](https://leetcode.cn/problems/add-two-numbers/)

参考的别人的解法

while(l1!=null||l2!=null){ 我这里如果用&&会导致一些位被丢弃

这玩意类似于把他抽象成竖式来计算

### [138. 随机链表的复制](https://leetcode.cn/problems/copy-list-with-random-pointer/)

这个题难点在于可能创建当前链表的时候listnode是空的，导致next和random给变成原来linklist的了，核心思路就是解耦，先创建一个hashmap，建立所有的val，random和next等全部都建立起来了之后再说。

### [19. 删除链表的倒数第 N 个结点](https://leetcode.cn/problems/remove-nth-node-from-end-of-list/)

这题如果删除的是头节点就会出错，然而只要设置guard就行了

