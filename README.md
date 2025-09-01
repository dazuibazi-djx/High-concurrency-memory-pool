# High-concurrency-memory-pool
google的tcmalloc核心代码部分的学习以及实现高效能的多线程内存池。当前项⽬是实现⼀个⾼并发的内存池，他的原型是google的⼀个开源项⽬tcmalloc，tcmalloc全称Thread-Caching Malloc，即线程缓存的malloc，实现了⾼效的多线程内存管理，⽤于替代系统的内存分配相关的函数（malloc、free）。 我们这个项⽬是把tcmalloc最核⼼的框架简化实现轻量高效的内存池，从而有效的解决高并发状态下的内存分配的问题。
