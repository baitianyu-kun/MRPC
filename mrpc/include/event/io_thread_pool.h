//
// Created by baitianyu on 7/19/24.
//

#ifndef RPCFRAME_IO_THREAD_POOL_H
#define RPCFRAME_IO_THREAD_POOL_H

#include <vector>
#include "common/log.h"
#include "event/io_thread.h"

#define MAX_THREAD_POOL_SIZE Config::GetGlobalConfig()->m_io_thread_pool_size

namespace mrpc {
    class IOThreadPool {
    public:
        explicit IOThreadPool(int size);

        ~IOThreadPool();

        void start();

        void join();

        void stop();

        std::unique_ptr<IOThread>& getIOThread();

    private:
        int m_pool_size{0};
        std::vector<std::unique_ptr<IOThread>> m_io_thread_pools;
        int m_index{0}; // 现在获取的是哪一个io thread
    };
}

#endif //RPCFRAME_IO_THREAD_POOL_H
