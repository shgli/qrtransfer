## 用途:从云桌面下载小的运维日志
## 使用:
1. 在云桌面准备好数据，重命名为x.tgz,放在sender同一级目录;
2. 在本地电脑上启动receiver, 点击browser,选择一个文件，用来存储接收到的数据;
3. 启动sender, 选择ack的模式, 二维码区域数量，放缩比例，点击start, 配置说明:
> 3.1. ack模式:   
> - ClipboardEvent sender监控剪贴板变化的事件，等待receiver的ack信息
> - PollClipboard sender每隔5毫秒获取clipboard的内容，看receiver是否ack
> - KeyboardEvent sender监控键盘消息，等待receiver的ack, 目前还没有实现  
> 如果可以从本地复制数据到云桌面，则优先选择ClipboardEvent或者PollClipboard模式, 云桌面是mac系统的情况下必须选择PollClipboard模式;如果Clipboard不可用，选择KeyboardEvent模式，目前还没有实现

> 3.2. 二维码区域数量:
> - 3x3表示一共9个二维码区域，每个区域3张二维码，一次可以传输27张二维码;
> - 3x3x1表示一共9个二维码区域，每个区域1张二维码，一次可以传输9张二维码, x1的部分只能为x1,x2或者x3。    
> 这个可以用来控制传输的速度，理论上一次传输的二维码数量越多速度越快，可是实际测试下来，达到一定数量后就没法提速了, 本地最大可以达到100多K/s, 真实环境差不多40K/S

> 3.3. 放缩比例, 最小为1，为2时面积是1时的4倍，对于画质不好的系统可能需要设置2或者3

4. 将sender放置在角落，保持可见，在选择clipboard模式时可以继续手头其他的工作, 选择Keyboard模式只能让sender一直获得焦点。

## 可能遇到的问题:
1. 选择了clipboard模式，但是不小心使用了剪贴板，导致传输中断，这时可以点击receiver上的ReAck按钮；
2. 传输过程中需要使用剪贴板，可以先将sender最小化，复制粘贴结束后再将sender可见；
3. 发现ReAck前显示ack详情的label中，]前面出现了N且后面跟着的数字一直在增长，同时显示的二维码越来越稀疏，需要stop，然后将放缩比例调大一点点，重新start;
4. receiver crash，stop sender,重新开始,这种情况不常见，还没有去找原因。
5. 放缩比例设置为2还是无法识别，将windows的显示比例设置为100%，再试试。

## 原理:
sender端将数据显示为二维码，receiver截屏识别二维码，然后通过剪切板通知识别完成，如此反复，直到接收完成

## 编译依赖: 
libpnl, libqrencode, 微信识别二维码的opencv扩展 zbar

## TODO:
1. 改进receiver中边界识别算法;
2. 开启一个新项目qrforward, 支持端口转发，从而实现云桌面内网程序与本机程序直接交互。
## <span style="color:red">使用前请征得系统管理员的同意，否则请立马删除本软件，因为使用本软件造成的一切后果由您自己承担</span>.
