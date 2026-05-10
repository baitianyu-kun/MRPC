# MRPC (Mini-RPC) Project

## Project Overview
MRPC is a high-performance asynchronous RPC (Remote Procedure Call) framework written in C++20. It draws inspiration from projects like Rocket and muduo. The framework supports both HTTP and a custom MPB protocol for communication, with RPC payload serialization handled by Protobuf.

Key features include:
- **Service Registration & Discovery:** Uses a Registration Center to maintain lists of active servers.
- **Load Balancing:** Implements consistent hashing with virtual nodes to distribute client requests.
- **Service Subscription/Notification:** Combines push and pull mechanisms to keep clients updated on service availability.
- **Heartbeat Detection:** Registration Center monitors server health using heartbeat packets.
- **Connection Management:** Reuses long connections for efficiency and supports Unix Domain Sockets for local communication.
- **Coroutine-based Asynchronous Invocation:** Provides asynchronous calling methods via native C++20 Coroutines (`co_await`), allowing developers to write asynchronous code using synchronous constructs.
- **Reactor Architecture:** Both the Registration Center and servers employ a Master-Slave Reactor model to handle connections and IO operations efficiently, fully integrated with C++20 coroutines for context switching.

## Core Technologies
- **Language:** C++20
- **Serialization:** Protobuf (Protocol Buffers)
- **Configuration:** XML (parsed via included TinyXML2)
- **Build System:** CMake

## Directory Structure
- `mrpc/`: Core framework source code and headers.
  - `include/`: Header files organized by module (`common`, `event`, `net`, `rpc`).
  - `src/`: Source files corresponding to the headers.
- `conf/`: Configuration files (e.g., `mrpc.xml`).
- `testcases/`: Test cases and examples demonstrating how to use the framework.
- `performance/`: Lua scripts and configurations for performance testing (e.g., using `wrk`).
- `assets/`: Images and resources used in documentation.

## Building and Running

### Dependencies
- **Protobuf:** Required for compiling `.proto` files and linking. (Version 3.19.4 is recommended in the README).
- **TinyXML2:** Included directly in the project source, no external installation required.

### Build Instructions
The project uses CMake. You can build it using the following commands from the root directory:

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Running Tests/Examples
After building, the executables will be available in the `build` directory. You can run the various components:

```bash
# Run the Registration Center
./test_rpc_register_center_subscribe_publish

# Run the RPC Server
./test_rpc_server

# Run the RPC Client (Channel)
./test_rpc_channel
```

### Adding New RPC Services
1. Define your service in a `.proto` file (e.g., `Service.proto`).
2. Generate C++ files using the Protobuf compiler:
   ```bash
   protoc --cpp_out=./ Service.proto
   ```
3. Implement the service logic based on the generated `.pb.cc` and `.pb.h` files.

## Performance Testing
Performance testing can be done using the `wrk` tool.
1. Update `conf/mrpc.xml` to set the protocol to HTTP and configure IO threads.
2. Run `wrk` with the provided Lua script:
   ```bash
   wrk -t 8 -c 1000 -d 30 --latency -s performance/performance.lua http://<server_ip>:<port>
   ```
   *(Ensure system file descriptors limit is appropriately high, e.g., `ulimit -n 5000`)*

## Development Conventions & Notes
- **Protocols:** Communication is handled via the `Protocol` abstract class, which is implemented for HTTP and the custom MPB protocol.
- **Event Loop:** The framework is built heavily around an `EventLoop` using `FDEvent` for managing file descriptors and callbacks.
- **Memory Management:** Extensive use of `std::shared_ptr` and `std::weak_ptr` for resource management. **Note:** Pay careful attention to avoid cyclic dependencies when capturing variables in callbacks (e.g., use `std::weak_ptr` when capturing `this` or other objects that might hold the callback).
- **Callbacks:** Request dispatching and handling use a `Servlet` pattern, specifically `CallBacksServlet` and `DispatchServlet`, relying on `std::bind` and lambda expressions.