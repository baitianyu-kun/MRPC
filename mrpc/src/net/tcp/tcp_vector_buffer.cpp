//
// Created by baitianyu on 7/19/24.
//
#include "net/tcp/tcp_vector_buffer.h"
#include "common/log.h"

namespace mrpc {

    TCPVectorBuffer::TCPVectorBuffer(int size) : m_size(size) {
        m_buffer.resize(size);
    }


    TCPVectorBuffer::~TCPVectorBuffer() {
        DEBUGLOG("~TCPVectorBuffer")
    }

    void TCPVectorBuffer::readFromBuffer(std::vector<char> &read_buff, int size) {
        if (readAbleSize() == 0) {
            return;
        }
        int read_size = readAbleSize() > size ? size : readAbleSize();
        std::vector<char> tmp(read_size);
        // m_buffer[m_read_index]相当于m_buffer + m_read_index，从这里开始的指针
        memcpy(&tmp[0], &m_buffer[m_read_index], read_size);
        read_buff.swap(tmp);
        // 更新read index
        m_read_index += read_size;
        adjustBuffer();
    }

    void TCPVectorBuffer::writeToBuffer(const char *buff, int size) {
        writeToBuffer(const_cast<char *>(buff), size);
    }

    void TCPVectorBuffer::writeToBuffer(char *buff, int size) {
        if (size > writeAbleSize()) {
            // 要写入的比剩余的空间大，扩容到1.5倍
            int new_size = (int) (1.5 * (m_write_index + size));
            resizeBuffer(new_size);
        }
        memcpy(&m_buffer[m_write_index], buff, size);
        m_write_index += size;
    }

    void TCPVectorBuffer::resizeBuffer(int new_size) {
        std::vector<char> tmp(new_size);
        // new size比可读的大的话，那么就拷贝可读的部分，否则截断可读的部分，拷贝一部分，即大小为new size的数据
        int count = new_size > readAbleSize() ? readAbleSize() : new_size;
        memcpy(&tmp[0], &m_buffer[m_read_index], count);
        m_buffer.swap(tmp);
        m_read_index = 0;
        m_write_index = count;
    }

    void TCPVectorBuffer::adjustBuffer() {
        // m read index不在数组最左边的时候，其左边的数据都读完了，不要了，所以需要把那部分删除掉，并把m read index归零
        // 这里设置的是如果m read index超过或者等于整个数组的三分之一就要进行重新调整
        if (m_read_index < int(m_buffer.size() / 3)) {
            return;
        }
        std::vector<char> tmp(m_buffer.size());
        int count = readAbleSize();
        // vector存储是连续的，获取具体元素的地址，由于是连续的，所以直接读取count个就可以
        memcpy(&tmp[0], &m_buffer[m_read_index], count);
        m_buffer.swap(tmp);
        m_read_index = 0;
        m_write_index = count;
        DEBUGLOG("adjustBuffer success, new size %d", count);
    }

    void TCPVectorBuffer::moveReadIndex(int size) {
        size_t new_index = m_read_index + size;
        if (new_index >= m_buffer.size()) {
            ERRORLOG("moveReadIndex error, invalid size %d, old_read_index %d, buffer size %d", size, m_read_index,
                     m_buffer.size());
            return;
        }
        m_read_index = new_index;
        adjustBuffer();
    }

    void TCPVectorBuffer::moveWriteIndex(int size) {
        size_t new_index = m_write_index + size;
        if (new_index >= m_buffer.size()) {
            ERRORLOG("moveReadIndex error, invalid size %d, old_read_index %d, buffer size %d", size, m_write_index,
                     m_buffer.size());
            return;
        }
        m_write_index = new_index;
        adjustBuffer();
    }

    void TCPVectorBuffer::clearBuffer() {
        m_read_index = 0;
        m_write_index = 0;
        m_buffer.clear();
    }

}


