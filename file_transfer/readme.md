# file transfer
简单的文件传输，某些特殊场景下，只能通过ssh隧道链接服务，所以想要传送文件非常复杂，因此写一个简单的问题传输工具，支持通过隧道进行传输。
一般情况下服务器都支持gcc,因此直接将server.c的文件通过控制台复制粘贴，在远程主机上编译运行服务。

# 使用方法
1. 远程主机启动server
```
gcc server.c -o server
```
启动执行:
```
./server -p 9999 -d ./cache
```
-p: 指定监听端口,表示  
-d: 指定存储的数据目录


2. 本地ssh建立隧道
使用ssh建立隧道
```
ssh -p 37177 -CNg -L 9999:localhost:9999 root@${remote_host}
```

3. 使用客户端上传
```
g++ client.c -o upload
./upload -p 9999 -f $file_uri
```

# TODO
1. 支持文件校验,SIMD加速校验过程
2. 支持断点续传
3. 支持可执行文件不落盘执行
4. 支持从服务器上交互下载文件


