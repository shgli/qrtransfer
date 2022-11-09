qrtransfer

用途:从云桌面下载small size的运维日志
使用:
    1. 在云桌面准备好数据，多个文件可以打包然后base64一下，重命名为x.tgz,放在sender同一级目录;
    2. 在本地电脑上启动receiver, 点击browser,选择一个文件，用来存储接收到的数据;
    3. 启动sender, 点击start。 注意: start后面有个edit box，表示不同的确认模式:
        a. 输入-1，表示让sender监听剪贴板改变的消息来确认(本地是windows时可以使用);
        b. 输入0或者不输入，表示让sender每隔50毫秒，去看看剪切板是否有确认的消息(本地是mac,windows时可以使用);
        c. 输入大于0的数N，表示不依赖剪切板确认，N毫秒后自动确认(剪切板不可以用时需用，需要使用者仔细设置)
    4. 一直保持sender界面可见直到接收完成.

原理:
    sender端将数据显示为二维码，receiver截屏识别二维码，然后通过剪切板通知识别完成，如此反复，直到接收完成

编译依赖: libpnl, libqrencode, 微信的二维码识别opencv扩展

