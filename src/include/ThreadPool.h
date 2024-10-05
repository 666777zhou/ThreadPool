#pragma once

#include <thread>
#include <vector>
#include <atomic>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <unordered_map>
#include <future>
#include <memory>


/*
    线程池构成
    1.管理者线程 -> 1个子线程
        -控制工作线程的数量
    2.若干工作线程 -> n个子线程
        -从任务队列里取任务
        -任务队列为空时，被条件变量阻塞
        -互斥锁实现线程同步
        -当前线程数量，空闲线程数量
        -最大、最小线程数量
    3.任务队列 -> stl.queue
        -互斥锁
        -条件变量
    4.线程池开关 -> bool
*/


class ThreadPool
{
public:
    ThreadPool(int min = 4, int max = std::thread::hardware_concurrency());
    ~ThreadPool();

    // 添加任务 -> 任务队列
    void addTask(std::function<void()> task);

    template<typename F, typename... Args>
    auto addTask(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;

private:
    void manager();
    void worker();

    std::thread m_manager; // 管理者线程
    // std::vector<std::thread::id> m_ids; // 存放将要被销毁的线程id
    std::unordered_map<std::thread::id, std::thread> m_workers; 
    std::atomic<int> m_maxNum;
    std::atomic<int> m_minNum;
    std::atomic<int> m_curNum;
    std::atomic<int> m_idleNum;
    std::atomic<int> m_exitNUm;
    std::atomic<bool> m_stop; // 线程池开关
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queueMtx;
    std::mutex m_mapMtx;
    std::condition_variable m_cv;
};

// 模板函数要写在头文件里
template <typename F, typename... Args>
inline auto ThreadPool::addTask(F &&f, Args &&...args) -> std::future<typename std::result_of<F(Args...)>::type>
{
    // 1.package_task
    using returnType = typename std::result_of<F(Args...)>::type; // using:给类型定别名
    auto mytask =  std::make_shared<std::packaged_task<returnType()>> (
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

    // 2.得到future对象
    std::future<returnType> res = mytask->get_future();

    // 3.任务函数添加到任务队列
    m_queueMtx.lock();
    m_tasks.emplace([mytask](){
        (*mytask)();
        });
    m_queueMtx.unlock();

    m_cv.notify_one(); // 通知消费者线程

    return res;
}
