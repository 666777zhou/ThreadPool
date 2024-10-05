#include "include/ThreadPool.h"


ThreadPool::ThreadPool(int min, int max) : 
            m_maxNum(max), 
            m_minNum(min), 
            m_stop(false), 
            m_curNum(max / 2), 
            m_idleNum(max / 2), 
            m_manager(&ThreadPool::manager, this)
{
    std::cout << "maxNum = " << max << std::endl;
    // 创建管理者线程
    // m_manager = new std::thread(&ThreadPool::manager, this);
    // 创建工作线程
    for(int i = 0; i < max / 2; i++)
    {
        // std::thread t(&ThreadPool::worker, this);
        // m_workers.emplace_back(std::thread(&ThreadPool::worker, this)); // 在emplace_back的括号里构建对象可以提高效率，否则与push_back一样
        std::thread t(&ThreadPool::worker, this);
        m_workers.insert({t.get_id(), move(t)}); // 线程对象不允许拷贝，只能采用move转移
    }
}

ThreadPool::~ThreadPool()
{
    m_stop = true;
    m_cv.notify_all();
    for(auto& it : m_workers) // 需用引用方式访问，因为线程对象不允许拷贝
    {
        if(it.second.joinable())
        {
            std::cout << "线程 " << it.second.get_id() << " 被销毁了" << std::endl;
            it.second.join();
        }
    }
    if(m_manager.joinable())
    {
        m_manager.join();
    }
    // delete m_manager;
}

void ThreadPool::addTask(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> locker(m_queueMtx);
        m_tasks.emplace(task);
    }
    m_cv.notify_one();
}

void ThreadPool::manager() // 检测并增删工作线程数量，
{
    while(!m_stop.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        int idle = m_idleNum.load();
        int cur = m_curNum.load();
        if(idle > cur / 2 && cur > m_minNum)
        {
            // 每次销毁两个线程
            m_exitNUm.store(2);
            m_cv.notify_all();
        }
        else if(idle == 0 && cur < m_maxNum)
        {
            // m_workers.emplace_back(std::thread(&ThreadPool::worker, this));
            std::thread t(&ThreadPool::worker, this);
            {
                std::lock_guard<std::mutex> locker(m_mapMtx);
                m_workers.insert({t.get_id(), move(t)});
            }
            m_curNum++;
            m_idleNum++;
        }
    }
}


void ThreadPool::worker()
{
    while(!m_stop.load())
    {
        std::function<void()> task = nullptr;
        {
            std::unique_lock<std::mutex> locker(m_queueMtx);
            while(m_tasks.empty() && !m_stop.load())
            {
                m_cv.wait(locker); // 此时互斥锁被打开
                if(m_exitNUm.load() > 0) // 删除线程
                {
                    m_curNum--;
                    m_exitNUm--;
                    m_idleNum--;
                    std::cout << "线程退出了, ID: " << std::this_thread::get_id() << std::endl;

                    std::thread::id this_id = std::this_thread::get_id();
                    {
                        std::lock_guard<std::mutex> locker(m_mapMtx);
                        auto it = m_workers.find(this_id);
                        if(it != m_workers.end())
                        {
                            std::cout << "线程 " << std::this_thread::get_id() << " 动态删除了" << std::endl;
                            it->second.detach();
                            m_workers.erase(it);
                        }
                    }
                    // m_ids.push_back(std::this_thread::get_id());
                    return;
                }
            }
            if(!m_tasks.empty())
            {
                std::cout << "取出了一个任务" << std::endl;
                task = move(m_tasks.front());
                m_tasks.pop();
            }     
        }
        if(task)
        {
            m_idleNum--;
            task();
            m_idleNum++;
     
        }
    }
}

void calc(int x, int y)
{
    int z = x + y;
    std::cout << "z = " << z << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

int calc1(int x, int y)
{
    int z = x + y;
    // std::cout << "异步函数处理了" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(4));
    return z;
}


int main()
{
    ThreadPool pool;
    std::vector<std::future<int>> res;
    for(int i = 0; i < 100; i++)
    {
        // auto func = std::bind(calc, i, i * 2);
        // pool.addTask(func);
        // std::cout << "添加了一个任务"  << std::endl;
        res.emplace_back(pool.addTask(calc1, i, i * 2));
    }
    for(auto& item : res)
    {
        std::cout << "线程执行的结果" << item.get() << std::endl;
        // std::cout << "异步函数处理了" << std::endl;
    }
    

    // getchar();

    return 0;
}




