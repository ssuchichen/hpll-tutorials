# Logging System
高性能日志系统，该系统设计用于低延迟地将日志写入内存缓冲区，并批量异步写入磁盘，适合需要高吞吐量日志记录的应用程序。

## 特点
* 低延迟：日志写入只操作内存，异步线程批量刷新到磁盘。
* 高性能：预分配大缓冲区，减少锁竞争（仅用原子操作）。
* 适用场景：服务器日志、调试信息收集。

## 优化点
* 用`mmap`映射文件直接写入磁盘。
* 增加多缓冲区切换机制（双缓冲或多缓冲）。
































