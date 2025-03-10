//
// Created by baitianyu on 7/19/24.
//

#ifndef RPCFRAME_TCP_VECTOR_BUFFER_H
#define RPCFRAME_TCP_VECTOR_BUFFER_H

#include <memory>
#include <vector>
#include <cstring>
#include "net/tcp/abstract_tcp_buffer.h"

#define MAX_TCP_BUFFER_SIZE Config::GetGlobalConfig()->m_tcp_buffer_size

namespace mrpc {

    // 主要是封装了流的读取写入等，包括inbuffer和outbuffer
    // inbuffer: 在fd可读的情况，调用read从socket缓冲区中读取到数据，然后写入到inbuffer里面
    // outbuffer： 在fd可写的情况调用write把outbuffer中的数据都写入到socket中
    class TCPVectorBuffer : public TCPBuffer {
    public:
        using ptr = std::shared_ptr<TCPVectorBuffer>;

        explicit TCPVectorBuffer(int size);

        ~TCPVectorBuffer();

        size_t getBufferSize() const { return m_buffer.size(); }

    public:
        void readFromBuffer(std::vector<char> &read_buff, int size) override;

        void writeToBuffer(char *buff, int size) override;

        void writeToBuffer(const char *buff, int size) override;

        int getReadIndex() const override { return m_read_index; }

        int getWriteIndex() const override { return m_write_index; }

        int readAbleSize() const override {return m_write_index - m_read_index;};

        int writeAbleSize() const override {return m_buffer.size() - m_write_index;};

        void clearBuffer();


    public:
        void resizeBuffer(int new_size);

        // read index左边的数据就不要了，整体都移动到开头，并设置read index为0，notion中回收调整
        void adjustBuffer();

        void moveReadIndex(int size);

        void moveWriteIndex(int size);

        // 返回buff的引用，方便对其进行操作
        std::vector<char> &getRefBuffer() { return m_buffer; }

        std::vector<char> getCopiedBuffer() const { return m_buffer; };

    private:
        int m_read_index{0};
        int m_write_index{0};
        int m_size{0};
        std::vector<char> m_buffer;
    };
}


#endif //RPCFRAME_TCP_VECTOR_BUFFER_H
