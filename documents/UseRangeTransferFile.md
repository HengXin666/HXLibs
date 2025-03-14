# 测试使用断点续传

- 启动: [使用`Transfer-Encoding`分块编码/`断点续传`传输文件的服务端](../examples/FileServer/FileServer.cpp)

## 一、wget测试

咱们使用`wget`测试:

1. 请求完整的文件

```sh
 root@Loli  ~ wget -c --no-check-certificate https://127.0.0.1:28205/test/range
--2025-03-13 00:12:11--  https://127.0.0.1:28205/test/range
Loaded CA certificate '/etc/ssl/certs/ca-certificates.crt'
Connecting to 127.0.0.1:28205... connected.
WARNING: The certificate of ‘127.0.0.1’ is not trusted.
WARNING: The certificate of ‘127.0.0.1’ doesn't have a known issuer.
The certificate's owner does not match hostname ‘127.0.0.1’
HTTP request sent, awaiting response... 200 OK
Length: 563873 (551K) [text/html]
Saving to: ‘range’

range                         100%[==============================================>] 550.66K  --.-KB/s    in 0.002s  

2025-03-13 00:12:11 (306 MB/s) - ‘range’ saved [563873/563873]
```

2. 删除文件的后面部分, 让wget从中断的地方继续下载 (模拟中断)

```sh
 root@Loli  ~ truncate -s 300K range
```

3. 继续请求完整的文件

```sh
 root@Loli  ~ wget -c --no-check-certificate https://127.0.0.1:28205/test/range
--2025-03-13 00:12:53--  https://127.0.0.1:28205/test/range
Loaded CA certificate '/etc/ssl/certs/ca-certificates.crt'
Connecting to 127.0.0.1:28205... connected.
WARNING: The certificate of ‘127.0.0.1’ is not trusted.
WARNING: The certificate of ‘127.0.0.1’ doesn't have a known issuer.
The certificate's owner does not match hostname ‘127.0.0.1’
HTTP request sent, awaiting response... 206 Partial Content
Length: 563873 (551K), 256673 (251K) remaining [text/html]
Saving to: ‘range’

range                         100%[+++++++++++++++++++++++++=====================>] 550.66K  --.-KB/s    in 0.001s  

2025-03-13 00:12:53 (456 MB/s) - ‘range’ saved [563873/563873]
```

可以发现它从中断的地方重新开始了, 而 **不是** 从头开始!

因此得证, 项目支持断点续传~

## 二、IDM测试

非常简单, 只需要点击下载即可:

![idm](./img/PixPin_2025-03-14_15-24-11.png)

> [!TIP]
> (图片是暂停的了, 已经测试了多次下载是可以继续的(断点续传嘛)) 目前服务器部署在VM上, 因此由于虚拟化等原因, 它的性能大大下降!
>
> *基准: 我通过vscode拖拽上传这个mp4文件的速度是在 8 ~ 9 MB/s 左右*