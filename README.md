# ThreadPool
基于C++11的异步线程池
## 线程池构成
1. 管理者线程 -> 1个子线程
    - 控制工作线程的数量

2. 若干工作线程 -> n个子线程
    - 从任务队列里取任务
    - 任务队列为空时，被条件变量阻塞
    - 互斥锁实现线程互斥
    - 当前线程数量，空闲线程数量
    - 最大、最小线程数量

3. 任务队列 -> `std::queue`
    - 互斥锁
    - 条件变量

4. 线程池开关 -> `bool`
