# MRPC 协程重构计划 (Coroutine Refactoring Plan)

## 1. 背景与目标
目前 MRPC 项目的异步逻辑大量依赖于 `std::function` 的回调机制（例如 `EventLoop::addTask`, `FDEvent::listen`, `TCPClient::connect`, `TCPConnection::sendRequest` 以及 `RPCChannel::callRPCAsync`）。这种做法虽然能实现非阻塞 I/O，但在复杂逻辑下（如长连接鉴权、重连、复杂的 RPC 业务流）极易导致“回调地狱”（Callback Hell），使得代码难以阅读、维护和调试，并容易引发循环引用（如项目中提及的 `FDEvent` 捕获 `TCPClient` 的问题）。

**重构目标：**
将整个项目的异步处理模型从**基于回调（Callback-based）**切换为**基于协程（Coroutine-based）**，使得业务代码能够以“同步的写法，异步的执行”。从而提升代码的可读性，简化内存生命周期管理，并降低开发新的 RPC 业务逻辑的门槛。

---

## 2. 技术选型
项目目前基于 C++14。要引入协程，主要有两个方向：
1. **升级到 C++20**：利用 C++20 标准提供的无栈协程（Stackless Coroutines: `co_await`, `co_yield`, `co_return`）。这是目前最现代、最推荐的 C++ 异步编程方式。
2. **使用第三方库**：如 `libco` (微信)、`Boost.Coroutine2` (有栈协程)。

**推荐方案：** 升级 CMake 构建配置至 C++20 并在底层事件循环中封装 C++20 标准协程。这样不需要引入厚重的第三方有栈协程库，且性能开销最小。后续步骤将基于 **C++20 无栈协程** 来展开。

---

## 3. 重构阶段与步骤拆解

### 阶段一：环境与基础设施升级
**目标**：开启 C++20 协程支持，并提供协程最基础的包装类（Task, Awaiter）。
- **步骤 1**：修改 `CMakeLists.txt` 和所有的子项目 CMake，将 `set(CMAKE_CXX_STANDARD 14)` 修改为 `set(CMAKE_CXX_STANDARD 20)`。
- **步骤 2**：引入基础协程类型（如 `Task<T>`），这通常涉及实现一个 `Promise` 类型来配合 C++20 编译器。
- **步骤 3**：保证所有原来的单测能跑通（仅升级编译器和标准）。
- **注意事项**：部分低版本的 GCC 可能会对协程支持不完善，建议 GCC 版本升级到至少 10.3（推荐 11.x 或以上）。

### 阶段二：底层事件循环 (EventLoop & FDEvent) 协程化适配
**目标**：将 `Epoll` 触发的文件描述符事件和定时器事件转化为可以 `co_await` 的对象。
- **步骤 1**：为 `FDEvent` 增加协程支持。之前的 `listen` 接收 `std::function`。现在需要实现一个 `SocketAwaiter`，当 `co_await SocketAwaiter` 时，将其挂起（`suspend`），并将协程句柄（`coroutine_handle`）注册到 `EventLoop` 的 `Epoll` 中。
- **步骤 2**：修改 `EventLoop` 的分发逻辑。当 `Epoll` 唤醒发现套接字可读/可写时，原先是执行 callback，现在是调用 `coroutine_handle.resume()` 恢复协程。
- **步骤 3**：同理，为定时器 `TimerQueue` 实现 `TimerAwaiter`，使得业务能通过 `co_await sleep_for(ms)` 异步挂起。
- **注意事项**：
  - 生命周期管理是重中之重。挂起的协程恢复时，需要确保之前引用的外部对象（如连接对象、缓冲对象）依然存活。
  - 需要妥善处理 `FDEvent` 上的并发读写（虽然是单线程的，但要处理不同协程对同一个 FD 的竞态）。

### 阶段三：网络层 (TCPClient & TCPConnection) 协程改造
**目标**：将所有网络收发操作（`connect`, `sendRequest`, `recvResponse`）改为协程。
- **步骤 1**：改造 `TCPClient::connect`。移除传入的 `std::function<void()>` done 参数。将其改造为返回 `Task<bool>` 或类似类型。在内部发起非阻塞 `connect` 后，通过 `co_await` 挂起，等待 `Epoll` 的可写事件唤醒。
- **步骤 2**：改造 `TCPConnection`。将 `m_read_dones` 和 `m_write_dones` 的 `std::function` 替换为协程句柄队列或者 `Awaiter` 对象。
- **步骤 3**：重构 `TCPConnectionPool`，将 `getConnectionAsync` 变为 `Task<TCPClient::ptr> getConnection()`。
- **注意事项**：由于移除了回调的层层嵌套，TCPClient 循环引用（导致 FD 泄露的问题）将不攻自破，因为协程函数内的栈变量可以严格控制在函数生命周期内自动销毁。

### 阶段四：RPC 层 (RPCChannel & RPCServer) 协程改造
**目标**：对用户侧屏蔽回调细节，使用户能通过一条语句完成 RPC 发送与接收。
- **步骤 1**：改造 `RPCChannel::callRPCAsync`。目前的实现是传入响应的 callback。需新增一个 `Task<std::shared_ptr<ResponseMsgType>> callRPC(RequestMsgType*)` 方法。
- **步骤 2**：重构 `RPCChannel::serviceDiscovery`，将向注册中心获取服务列表的过程改为协程同步写法。
- **步骤 3**：服务端改造。目前 `DispatchServlet` 派发任务也是一个普通函数调用。可以为服务端提供协程上下文，使得开发者在编写 `Servlet` 处理逻辑时可以 `co_await` 其他服务（即支持微服务之间相互调用的协程化）。
- **注意事项**：保留兼容老代码的 `Future` 模式，但在框架内主推 `co_await`。

### 阶段五：清理与性能测试
**目标**：剔除废弃的 callback 代码，验证性能。
- **步骤 1**：删除项目中多余的 `std::function` 回调签名，清理不再使用的旧接口。
- **步骤 2**：运行 `wrk` 测试脚本，对比协程版本与 Callback 版本的吞吐量（QPS）和延迟情况。协程通常会有微小的上下文切换和分配开销，需要监控是否可以接受。
- **步骤 3**：使用 Valgrind/ASAN 进行内存泄漏排查，确保没有因为协程句柄未销毁导致的内存泄露。

---

## 4. TODO List

### 环境准备
- [x] 修改项目所有 `CMakeLists.txt`，更新 `CMAKE_CXX_STANDARD` 为 20。
- [x] 确保开发环境的 GCC/Clang 版本支持 C++20 协程，并编译运行一遍现有测试。
- [x] 引入或编写基本的协程抽象工具类：`Task`, `Promise`, 并且能顺利编译通过。

### 事件驱动层重构
- [x] 编写 `SocketAwaiter`，用于封装套接字的可读/可写等待。
- [x] 修改 `EventLoop` 的 `Epoll` 唤醒逻辑，支持协程的 `resume()`。
- [x] 编写 `TimerAwaiter` 协程睡眠支持，并重构 `TimerQueue`。

### 网络模块重构
- [x] 改造 `TCPClient::connect()`，移除 callback。
- [x] 改造 `TCPConnection::sendRequest()` 和 `recvResponse()`，支持 `co_await` 进行报文收发。
- [x] 改造 `TCPConnectionPool`。

### RPC 服务发现与调用重构
- [x] 将客户端获取注册中心地址列表 (`RegisterCenter::serviceDiscovery`) 协程化。
- [x] 将 `RPCChannel` 的远端调用从 `callRPCAsync` 和 `callRPCFuture` 简化为 `Task<Response> call()`。
- [x] 改造服务端的 `DispatcherServlet` 支持返回 `Task<void>`，以便服务端业务逻辑也能异步。

### 清理与发布
- [x] 重构单元测试用例，用 `Task` 的方式编写所有的 `testcases`。
- [x] 补充协程异常处理（`try-catch` 捕捉协程中的连接断开异常）。
- [x] 性能压测对比，编写测试报告。
- [x] 更新 README 与 GEMINI.md 文档，明确项目现在已变更为 C++20 协程驱动框架。