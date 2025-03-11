//
// Created by baitianyu on 7/19/24.
//

#ifndef RPCFRAME_IO_THREAD_H
#define RPCFRAME_IO_THREAD_H

#include <vector>
#include <pthread.h>
#include <semaphore.h> // 信号量用来同步
#include "common/log.h"
#include "event/eventloop.h"


namespace mrpc {
    class IOThread {
    public:
        IOThread();

        ~IOThread();

        EventLoop::ptr getEventLoop();

        void start();

        void join();

        void stop();

    public:
        static void *runner(void *arg);

    private:
        pid_t m_thread_id{-1};
        pthread_t m_thread{0}; // 线程句柄，用来保存和操作线程的

        EventLoop::ptr m_event_loop{nullptr};
        // 要求IOThread创建线程时候，等待执行完runner的event loop的loop方法之前，即完成event loop创建任务，但是不启动，放在start中启动
        // 随后runner阻塞在启动loop前，等start给信号后才启动
        sem_t m_init_semaphore;
        // start方法信号量+1，随后runner继续开始执行loop方法
        sem_t m_start_semaphore;
    };
}


#endif //RPCFRAME_IO_THREAD_H
