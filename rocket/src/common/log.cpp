#include <sys/time.h>
#include <sstream>
#include <cstdio>
#include <cassert>
#include "common/log.h"
#include "common/util.h"
#include "common/config.h"

#define TIME_ARRAY_SIZE 32

namespace rocket {

    static Logger *g_logger = NULL;

    Logger *Logger::GetGlobalLogger() {
        return g_logger;
    }

    void Logger::InitGlobalLogger() {

        LogLevel global_log_level = StringToLogLevel(Config::GetGlobalConfig()->m_log_level);
        printf("Init log level [%s]\n", LogLevelToString(global_log_level).c_str());
        g_logger = new Logger(global_log_level);

    }

    std::string LogLevelToString(LogLevel level) {
        switch (level) {
            case Debug:
                return "DEBUG";

            case Info:
                return "INFO";

            case Error:
                return "ERROR";
            default:
                return "UNKNOWN";
        }
    }

    LogLevel StringToLogLevel(const std::string &log_level) {
        if (log_level == "DEBUG") {
            return Debug;
        } else if (log_level == "INFO") {
            return Info;
        } else if (log_level == "ERROR") {
            return Error;
        } else {
            return Unknown;
        }
    }

    std::string LogEvent::toString() {
        struct timeval now_time;

        gettimeofday(&now_time, nullptr);

        struct tm now_time_t;
        localtime_r(&(now_time.tv_sec), &now_time_t);

        char buf[128];
        strftime(&buf[0], 128, "%y-%m-%d %H:%M:%S", &now_time_t);
        std::string time_str(buf);
        int ms = now_time.tv_usec / 1000;
        time_str = time_str + "." + std::to_string(ms);


        m_pid = getPid();
        m_thread_id = getThreadId();

        std::stringstream ss;

        ss << "[" << LogLevelToString(m_level) << "]\t"
           << "[" << time_str << "]\t"
           << "[" << m_pid << ":" << m_thread_id << "]\t";

        return ss.str();
    }

    void Logger::pushLog(const std::string &msg) {
        ScopeMutext<Mutex> lock(m_mutex);
        m_buffer.push(msg);
        lock.unlock();

    }

    void Logger::log() {

        ScopeMutext<Mutex> lock(m_mutex);
        std::queue<std::string> tmp;
        m_buffer.swap(tmp);

        lock.unlock();

        while (!tmp.empty()) {
            std::string msg = tmp.front();
            tmp.pop();
            printf(msg.c_str());
        }

    }

    AsyncLogger::AsyncLogger(const std::string &file_name, const std::string &file_path, int max_size) : m_file_name(
            file_name), m_file_path(file_path), m_max_file_size(max_size) {
        // 初始化sem，后两个为0代表是当前线程的局部信号量，否则线程之间共享
        assert(sem_init(&m_semaphore, 0, 0) == 0);
        assert(pthread_create(&m_thread, nullptr, AsyncLogger::Loop, this) == 0);
        // wait，直到新的线程执行完创建logger，保证loop创建完后才认为是创建成功
        sem_wait(&m_semaphore);
    }

    AsyncLogger::~AsyncLogger() {

    }

    void AsyncLogger::stop() {
        m_stop_flag = true;
    }

    // 强制用fflush进行刷新
    void AsyncLogger::flush() {
        if (m_file_handler) {
            fflush(m_file_handler);
        }
    }

    void AsyncLogger::pushLogBuffer(std::vector<std::string> &vec) {
        ScopeMutext<Mutex> lock(m_mutex);
        m_buffer.emplace(vec);
        pthread_cond_signal(&m_condition); // 唤醒线程，去执行loop函数
        lock.unlock();
    }

    void *AsyncLogger::Loop(void *arg) {
        // 将buffer里面的数据全部打印到文件中，然后线程睡眠，直到有新的数据再重复这个过程
        auto logger = reinterpret_cast<AsyncLogger *>(arg);
        assert(pthread_cond_init(&logger->m_condition, nullptr) == 0);
        // 增加信号量，使主线程从AsyncLogger的构造函数中正常结束，主线程保证loop创建完后才认为是创建成功
        sem_post(&logger->m_semaphore);

        while (true) {
            ScopeMutext<Mutex> lock(logger->m_mutex);

            // 不能只检查一次条件，应该在每次唤醒的时候都进行检测

            // while 循环：while 循环确保在等待期间检查条件，以防止虚假唤醒（spurious wakeups）。
            // 在多线程编程中，线程可能会在没有明确收到信号的情况下醒来（虚假唤醒），
            // 因此需要在循环中检查条件，并在条件满足时才继续执行。

            // pthread_cond_signal在多处理器上可能同时唤醒多个线程，当你只能让一个线程处理某个任务时，
            // 其它被唤醒的线程就需要继续wait,while循环的意义就体现在这里了，
            // pthread_cond_signal()也可能唤醒多个线程，而如果你同时只允许一个线程访问的话，
            // 就必须要使用while来进行条件判断，以保证临界区内只有一个线程在处理
            while (logger->m_buffer.empty()) {
                pthread_cond_wait(&(logger->m_condition), logger->m_mutex.getMutex());
            }
            // 保存logger的头部
            std::vector<std::string> tmp;
            tmp.swap(logger->m_buffer.front());
            logger->m_buffer.pop();

            lock.unlock();

            // 时间
            timeval now;
            gettimeofday(&now, nullptr);

            struct tm now_time;
            localtime_r(&(now.tv_sec), &now_time);

            const char *format = "%Y%m%d";
            char date[TIME_ARRAY_SIZE];
            strftime(date, sizeof(date), format, &now_time);

            if (std::string(date) != logger->m_date) {
                logger->m_no = 0;
                logger->m_reopen_flag = true;
                logger->m_date = std::string(date);
            }
            // 文件handler已经关闭了的话需要重新打开
            if (logger->m_file_handler == nullptr) {
                logger->m_reopen_flag = true;
            }

            std::stringstream ss;;
            // <<就是往流里面写入东西，<<就是从流往外面输出东西，可以到int，string等常见类型，并可以互相进行类型转换
            // stringstream stream;
            // stream<<t;//向流中传值
            // out_type result;//这里存储转换结果
            // stream>>result;//向result中写入值
            ss << logger->m_file_path << logger->m_file_name << "_" << std::string(date) << "_log.";
            std::string log_file_name = ss.str() + std::to_string(logger->m_no);
            if (logger->m_reopen_flag) {
                if (logger->m_file_handler) {
                    fclose(logger->m_file_handler);
                }
                logger->m_file_handler = fopen(log_file_name.c_str(), "a");
                logger->m_reopen_flag = false;
            }
            // ftell返回给定流 stream 的当前文件位置，即字节数
            if (ftell(logger->m_file_handler) > logger->m_max_file_size) {
                fclose(logger->m_file_handler);
                log_file_name = ss.str() + std::to_string(logger->m_no++);
                logger->m_file_handler = fopen(log_file_name.c_str(), "a");
                logger->m_reopen_flag = false;
            }
            // 进行输出
            for (const auto &item: tmp) {
                if (!item.empty()) {
                    fwrite(item.c_str(), 1, item.length(), logger->m_file_handler);
                }
            }
            fflush(logger->m_file_handler);
            if (logger->m_stop_flag) {
                // 进行退出
                return nullptr;
            }
        }
        return nullptr;
    }
}