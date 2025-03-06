### [224. 基本计算器](https://leetcode.cn/problems/basic-calculator/)

这一题我也不太会，而核心思路是遇到 （  就把之前的保存，遇到）就弹出符号和之前的结果运算，最后实现累加

### [71. 简化路径](https://leetcode.cn/problems/simplify-path/)

先分割字符然后一个个入栈，当 / 在开头时或者//连着会有空格，记得检测一下，检测到 ..弹出，其他情况入栈即可

### [20. 有效的括号](https://leetcode.cn/problems/valid-parentheses/)

建造一个hashmap，然后通过pairs.get(x)来确定是不是弹出 '（'  对应 '）'
