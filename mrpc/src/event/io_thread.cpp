//
// Created by baitianyu on 7/19/24.
//
#include "event/io_thread.h"
#include "common/util.h"

namespace mrpc {
    mrpc::IOThread::IOThread() {
        // 初始化sem，后两个为0代表是当前线程的局部信号量，否则线程之间共享
        assert(sem_init(&m_init_semaphore, 0, 0) == 0);
        assert(sem_init(&m_start_semaphore, 0, 0) == 0);

        // 创建线程
        // 类的成员函数在调用时除了传进参数列表的参数之外，还会再传入一个this指针，
        // 指向调用此函数的对象。只有传入这个this指针，函数在执行过程中才能调用其他非静态成员变量和非静态成员函数。
        // 因此，如果我们的pthread_create函数如果传进了一个形如 void* func(void*)的非静态成员函数，
        // 那么恭喜你，程序将会报错，因为编译器在你看不见的地方还给你附加了一个参数this*。
        // 只能传入一个静态函数了。静态函数没有this指针。
        // 但是如何访问对象里面的成员呢？
        // 1. 通过类的静态对象来调用。比如单例模式中，静态函数就可以通过唯一的那个静态实例对象来调用非静态成员。
        // 2. 自己在函数里创建一个对象，自己new一个
        // 3. 可以通过传入一个this指针，然后强转换为当前对象就可以，例如下面。
        // arg是runner的参数
        pthread_create(&m_thread, nullptr, IOThread::runner, this);

        // wait，直到新的线程执行完runner的前置，即loop之前，否则还没getThreadId就进行debug打印会出问题
        sem_wait(&m_init_semaphore); // 如果信号量为0的话阻塞，否则跳出阻塞，信号量同时--
        DEBUGLOG("IOThread [%d] create success", m_thread_id);
    }

    IOThread::~IOThread() {
        m_event_loop->stop();
        sem_destroy(&m_init_semaphore);
        sem_destroy(&m_start_semaphore);
        join();
        DEBUGLOG("~IOThread");
    }

    EventLoop::ptr IOThread::getEventLoop() {
        return m_event_loop;
    }

    void IOThread::start() {
        // 信号量++，执行完之后，runner中监听启动的停止阻塞，开始执行loop方法
        // 在这里直接让thread启动loop的话，应该也可以，但是应该是在runner中启动，比较符合每个线程启动自己的方法的逻辑
        // 在这里m_event_loop->loop()确实是可以的，已经做过了测试
        DEBUGLOG("Now invoke IOThread [%d]", m_thread_id);
        sem_post(&m_start_semaphore);
    }

    void IOThread::join() {
        pthread_join(m_thread, nullptr);
    }

    void *IOThread::runner(void *arg) {
        // 前置任务
        auto thread = reinterpret_cast<IOThread *>(arg);
        thread->m_event_loop = EventLoop::GetCurrentEventLoop();
        thread->m_thread_id = getThreadId();
        // 初始化完成，等待开启信号
        sem_post(&thread->m_init_semaphore);
        DEBUGLOG("IOThread %d created, wait start semaphore", thread->m_thread_id);
        sem_wait(&thread->m_start_semaphore);
        // 接收到了开启信号
        DEBUGLOG("IOThread %d start loop ", thread->m_thread_id);
        thread->m_event_loop->loop();
        DEBUGLOG("IOThread %d end loop ", thread->m_thread_id);
        return nullptr; // void*的让他返回nullptr就可以
    }

    void IOThread::stop() {
        m_event_loop->stop();
    }
}

