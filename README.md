# Proxy Server with Thread Pool

This project includes a proxy server implementation (`proxyServer.c`) along with a custom thread pool (`threadpool.h` and `threadpool.c`). The proxy server handles HTTP requests, socket operations, error handling, and connection management, while the thread pool provides a mechanism for managing concurrent tasks efficiently.

---

**Description:**

The project combines a proxy server and a thread pool to manage concurrent connections effectively. The proxy server handles incoming HTTP requests, processes them, and forwards them to the appropriate destination, acting as an intermediary between clients and servers. The thread pool ensures optimal utilization of system resources by managing multiple tasks concurrently.

---

**Project Structure:**

1. **`proxyServer.c`:**
   - Implements the proxy server functionality, including HTTP request handling, socket operations, error management, and connection handling.

2. **`threadpool.h` and `threadpool.c`:**
   - Define and implement a custom thread pool that manages concurrent tasks efficiently. The thread pool handles task queuing, thread creation, task execution, and resource cleanup.

---

**Usage:**

1. Compile the project using a C compiler:
   ```bash
   gcc proxyServer.c threadpool.c -o proxyServer -pthread
   ```
2. Run the compiled executable:
   ```bash
   ./proxyServer
   ```
3. Configure client applications to use the proxy server for HTTP requests.

---

**Features:**

- Proxy server functionality with HTTP request handling.
- Thread pool for managing concurrent tasks and improving performance.
- Socket operations for communication between clients and servers.
- Error handling and connection management.

---


---

**Note:**
Replace `YourUsername` in the example image URL with your actual GitHub username or repository path where the project is hosted.
