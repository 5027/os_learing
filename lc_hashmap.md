### [383. 赎金信](https://leetcode.cn/problems/ransom-note/)

我是用hashmap写的

实际上可以首先统计 magazine 中每个英文字母 a 的次数 cnt[a]，再遍历统计 ransomNote 中每个英文字母的次数，如果发现 ransomNote 中存在某个英文字母 c 的统计次数大于 magazine 中该字母统计次数 cnt[c]，则此时我们直接返回 false。

### [205. 同构字符串](https://leetcode.cn/problems/isomorphic-strings/)

将第一个出现的字母映射成 1，第二个出现的字母映射成 2

对于 egg
e -> 1
g -> 2
也就是将 egg 的 e 换成 1, g 换成 2, 就变成了 122

我采用得方法是hashmap把俩字符串映射

### [290. 单词规律](https://leetcode.cn/problems/word-pattern/)

原来的思路是对的，测例有点恶心，随便抄了个题解

```
class Solution {
  public boolean wordPattern(String pattern, String s) {
​    String[] str = s.split(" "); 
​    HashMap<String,Character> map = new HashMap<>();
​    StringBuilder s1 = new StringBuilder();
​    int count=0;
​    int n=str.length;
​    for(int i=0;i<n;i++){
​      if(map.containsKey(str[i])){
​        s1.append( map.get(str[i]));
​      }else{
​        count++;
​        char c = (char) ('a' + (count- 1));
​        map.put(str[i],c);
​        s1.append( map.get(str[i]));
​      }
}  
 return s1.toString().equals(pattern);
  }
}
```

看了看别人的题解，可以直接用字符串匹配

```
class Solution {
    public boolean wordPattern(String pattern, String s) {
        List<String> ls = Arrays.asList(s.split(" "));
        int n = pattern.length();
        if (n != ls.size()) return false;
        for (int i = 0; i < n; i++) {
            if (pattern.indexOf(pattern.charAt(i)) != ls.indexOf(ls.get(i)))
                return false;		
        }
        return true;
    }
}
```

### [1. 两数之和](https://leetcode.cn/problems/two-sum/)

我们创建一个哈希表，对于每一个 `x`，我们首先查询哈希表中是否存在 `target - x`，然后将 `x` 插入到哈希表中，即可保证不会让 `x` 和自己匹配。

### [202. 快乐数](https://leetcode.cn/problems/happy-number/)

重要的问题在于检测循环。

因此我们在这里可以使用弗洛伊德循环查找算法。这个算法是两个奔跑选手，一个跑的快，一个跑得慢。在龟兔赛跑的寓言中，跑的慢的称为 “乌龟”，跑得快的称为 “兔子”。

不管乌龟和兔子在循环中从哪里开始，它们最终都会相遇。这是因为兔子每走一步就向乌龟靠近一个节点（在它们的移动方向上）。

### [128. 最长连续序列](https://leetcode.cn/problems/longest-consecutive-sequence/)

我犯了两个错误

一个是count应该初始化为1

而且else if (nums[i] != nums[i +1]) 因为可能出现连续相同的数字

### [56. 合并区间](https://leetcode.cn/problems/merge-intervals/)

区间这几题我都没写……

### [452. 用最少数量的箭引爆气球](https://leetcode.cn/problems/minimum-number-of-arrows-to-burst-balloons/)

思路
从左到右思考，第一个点（最左边的点）放在哪最好？

例如 points=[[1,6],[6,7],[2,8],[7,10]]。第一个点放在 x=1 处，只能满足一个区间 [1,6]，而如果放在 x=2 处，则可以满足区间 [1,6],[2,8]。进一步地，如果放在更靠右的 x=6 处，则可以满足三个区间 [1,6],[6,7],[2,8]。注意不能放在 x=7 处，这样没法满足区间 [1,6] 了。

在 x=6 放点后，问题变成剩下 n−3 个区间，在数轴上最少要放置多少个点，使得每个区间都包含至少一个点。这是一个规模更小的子问题，可以用同样的方法解决。

