# 优化DNF内置ACE模块
论坛链接：https://bbs.colg.cn/thread-8518972-1-1.html
## 工作原理
修改ACE-GDPServer64.dll中的sub_180004340()函数：<br>
将WaitForSingleObject等待结束后执行的Sleep的参数由0修改为100。<br><br>
【注】使用0调用Sleep仅仅意味着线程放弃当前时间片，并不意味着节省系统资源。
事实上此时资源已经几乎全部消耗在上下文转换（以及用户-内核态切换）中。