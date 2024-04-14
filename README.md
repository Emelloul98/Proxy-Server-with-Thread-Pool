# Proxy Server with Thread Pool

This project combines a proxy server implementation (`proxyServer.c`) with a custom thread pool (`threadpool.h` and `threadpool.c`) to manage concurrent connections efficiently. The proxy server handles HTTP requests, socket operations, error handling, and connection management, while the thread pool provides a mechanism for managing multiple tasks concurrently.

---

**Description:**

The project implements a proxy server that acts as an intermediary between clients and servers, forwarding HTTP requests and responses. It utilizes a custom thread pool to handle concurrent connections effectively, optimizing resource usage and performance.

---

**Project Structure:**

1. **`proxyServer.c`:**
   - Implements the proxy server functionality.
   - Accepts command-line arguments for configuration:
     - Port number: Specifies the port on which the proxy server listens for incoming connections.
     - Max connections: Sets the maximum number of concurrent connections the server can handle.
     - Cache size: Defines the size of the cache for storing recently accessed resources.

2. **`threadpool.h` and `threadpool.c`:**
   - Define and implement a custom thread pool for managing concurrent tasks efficiently.
   - Handles task queuing, thread creation, task execution, and resource cleanup.

---

**Usage:**

1. Compile the project using a C compiler:
   ```bash
   gcc proxyServer.c threadpool.c -o proxyServer -pthread
   ```
2. Run the compiled executable with command-line arguments:
   ```bash
   ./proxyServer <port> <max_connections> <cache_size>
   ```
   - Replace `<port>`, `<max_connections>`, and `<cache_size>` with appropriate values.
   - Example: `./proxyServer 8080 100 50`

3. Configure client applications to use the proxy server for HTTP requests.

---

**Features:**

- Proxy server functionality with HTTP request handling.
- Customizable parameters for port number, maximum connections, and cache size.
- Thread pool for managing concurrent tasks and improving performance.
- Socket operations for communication between clients and servers.
- Error handling and connection management.

---
