# High-concurrency-memory-pool
  我当前的项目目标是实现一个高并发环境下的高效内存池，其原型参考了 Google 开源的 tcmalloc（Thread-Caching Malloc）。tcmalloc 的核心思想是为每个线程维护本地缓存，从而减少线程间竞争，提高内存分配与释放的效率。基于这一思路，我们在保留其关键机制的基础上，对框架进行了简化，实现了一个轻量级的内存池，以有效解决多线程场景下的内存分配瓶颈问题。
