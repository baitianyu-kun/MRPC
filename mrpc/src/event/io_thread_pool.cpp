//
// Created by baitianyu on 7/19/24.
//
#include "event/io_thread_pool.h"

namespace mrpc {


    IOThreadPool::IOThreadPool(int size) {
        m_io_thread_pools.resize(size);
        for (auto &m_io_thread: m_io_thread_pools) {
            m_io_thread = std::make_unique<IOThread>();
        }
    }

    IOThreadPool::~IOThreadPool() {

    }

    void IOThreadPool::start() {
        for (const auto &m_io_thread: m_io_thread_pools) {
            m_io_thread->start();
        }
    }

    void IOThreadPool::join() {
        for (const auto &m_io_thread: m_io_thread_pools) {
            m_io_thread->join();
        }
    }

    std::unique_ptr<IOThread> &IOThreadPool::getIOThread() {
        if (m_index == m_io_thread_pools.size()) {
            m_index = 0;
        }
        return m_io_thread_pools[m_index++];
    }

    void IOThreadPool::stop() {
        for (const auto &m_io_thread: m_io_thread_pools) {
            m_io_thread->stop();
        }
    }

}
