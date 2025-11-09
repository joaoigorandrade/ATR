## 1. Introduction to Real-Time Automation
* **Course Syllabus:** Presentation of the objectives, syllabus, schedule, assessments (exams, computational exercises, final project), and bibliography of the course.
* **Real-Time Concepts:** Definition of real-time systems (RTS), where correctness depends both on the logical result and the time at which it is produced.
* **RTS Classification:** Differentiation between *Hard Real-Time* systems (catastrophic failures if deadlines are missed) and *Soft Real-Time* systems (performance degradation if deadlines are missed).
* **Applications:** Examples of where RTS are found, such as automotive systems (ABS, airbag), industrial process control, aerospace systems (fly-by-wire), robotics, and consumer electronics.
* **System Architecture:** Evolution from the automation pyramid (ISA-95) to networked architectures (IIoT, Industry 4.0) and the challenge of maintaining critical timing requirements with increased connectivity.

## 2. Fundamentals of Operating Systems (OS)
* **Role of the OS:** Viewed as an "Arbiter" (manages resources), "Illusionist" (abstracts hardware), and "Glue" (provides common services).
* **Kernel Abstractions:** Introduction to the core concepts managed by the kernel:
    * Processes
    * Threads
    * Address Space
    * Files and Sockets
* **Operation Modes:** Differentiation between *User Mode* (applications) and *Kernel Mode* (OS core), which ensures system protection.
* **System Calls:** Interface mechanism for an application in *User Mode* to request services from *Kernel Mode* (e.g., `open()`, `read()`, `fork()`).
* **Virtualization:** Concepts of Virtual Machines (VMs) and Containers (e.g., Docker, WSL).

## 3. Process Management
* **Process:** Definition as a program in execution, possessing a protected address space (memory) and one or more threads.
* **Address Space:** Structure of a process’s memory (segments `text`, `data`, `heap`, `stack`).
* **Process States:** Lifecycle of a process (`new`, `ready`, `running`, `waiting`, `terminated`).
* **Process Control Block (PCB):** Data structure used by the kernel to store a process’s context (ID, registers, PC, state, etc.).
* **Process Creation (POSIX):** Use of the calls `fork()` (creates a copy of the process), `exec()` (replaces the current program with a new one), and `wait()` (parent process waits for the child).
* **Context Switching:** Process by which the CPU switches execution between different processes or threads, saving and restoring their states.

## 4. Thread Management
* **Thread:** Definition as a basic unit of execution (a "flow") within a process. Threads of the same process share memory (`code`, `data`, `heap`), but each has its own stack (`stack`), program counter (PC), and registers.
* **Advantages:** They are "lighter" to create and manage than processes, enabling concurrency and parallelism more efficiently.
* **Concurrent vs. Parallel Programming:** Distinction between concurrency (tasks managed "simultaneously") and parallelism (tasks actually executing at the same time on multiple cores).
* **POSIX Threads (pthreads):** Standard C API for thread manipulation.
    * Main functions: `pthread_create()`, `pthread_join()`, `pthread_exit()`, `pthread_detach()`.
* **C++ Threads:** Native thread support in the C++ standard library (since C++11).
    * Classes and functions: `std::thread`, `join()`, `detach()`.

## 5. Synchronization of Threads and Processes
* **Race Condition:** Problem that occurs when the result of an operation depends on the unpredictable order in which multiple threads access and modify shared data (e.g., ATM problem).
* **Critical Section Problem:** How to ensure that only one thread at a time executes a code block that accesses shared resources.
* **Hardware Solutions:** Atomic instructions provided by the CPU, such as `TestAndSet` and `CompareAndSwap`.
* **Mutex (Mutual Exclusion):** Synchronization object used to ensure mutual exclusion. It is the most basic tool for protecting a critical section.
    * Operations: `lock()` (locks) and `unlock()` (unlocks).
    * Implementations: `pthread_mutex_t` (POSIX) and `std::mutex` (C++).
    * **RAII (Resource Acquisition Is Initialization):** Design pattern in C++ for safe resource management, exemplified by `std::lock_guard` and `std::unique_lock`, which automatically release the mutex at the end of the scope.
* **Semaphores:** More general synchronization mechanism (created by Dijkstra).
    * Operations: `wait()` (or P, decrements the counter and blocks if zero) and `signal()` (or V, increments the counter and wakes a blocked thread).
    * Types: **Binary** (value 0 or 1, similar to a mutex) and **Counting** (value 0 to N, used to manage a resource *pool*).
    * Implementations: `sem_t` (POSIX) and `std::counting_semaphore` / `std::binary_semaphore` (C++20).
* **Monitors and Condition Variables:** High-level mechanism that combines a mutex with wait queues, allowing threads to wait for a specific condition (*without* busy-waiting).
    * Operations: `wait()` (atomically releases the mutex and blocks the thread until notified), `notify_one()` (wakes one waiting thread), `notify_all()` (wakes all).
    * Implementations: `pthread_cond_t` (POSIX) and `std::condition_variable` (C++).

## 6. Classic Synchronization Problems and Deadlock
* **Classic Problems:** Standardized scenarios used to test and demonstrate the effectiveness of synchronization mechanisms.
    * **Producers and Consumers (Bounded Buffer):** Coordination of threads that add items (producers) and remove items (consumers) from a shared buffer, preventing producers from adding to a full buffer or consumers from removing from an empty buffer.
    * **Readers and Writers:** Management of access to a resource where multiple "reader" threads can access simultaneously, but "writer" threads require exclusive access.
    * **Dining Philosophers:** Classic problem used to illustrate the risk of *deadlock*.
    * **Sleeping Barber:** Synchronization of clients and a barber with a finite waiting room.
    * **Dining Savages:** Variation of the Producer-Consumer problem.
* **Deadlock:** Situation in which two or more threads are permanently blocked, each waiting for a resource held by the other.
    * **4 Conditions for Deadlock:** Mutual Exclusion, Hold and Wait, No Preemption, and Circular Wait.
    * **Handling:** Strategies such as prevention (e.g., ordering lock acquisition), detection and recovery, or simply ignoring it (the most common approach in general-purpose OSes).

## 🧭 Introduction and Fundamental Concepts
* [cite_start]**Course Syllabus:** Presentation of the course, syllabus, assessments, bibliography (books by Silberschatz, Tanenbaum, Buttazzo, etc.), and tools[cite: 15452, 15531, 15539, 15549, 15712, 15723].
* [cite_start]**Real-Time Systems (RTS):** Definition of systems where correctness depends not only on the logical result but also on the time it is produced[cite: 16071].
* **RTS Classification:** Differentiation between:
    * [cite_start]**Hard Real-Time:** Missing a deadline is catastrophic[cite: 16308].
    * [cite_start]**Soft Real-Time:** Missing a deadline leads to performance degradation but is not catastrophic[cite: 16305].
    * [cite_start]**Firm Real-Time:** A late result is useless but causes no damage[cite: 6656].
* [cite_start]**Applications:** Examples in industrial processes, embedded systems (automotive, aerospace), robotics, navigation, and multimedia[cite: 41, 15913, 15924, 15858, 15873].
* [cite_start]**Concurrency vs. Parallelism:** Distinction between concurrency (managing multiple tasks at the same time) and parallelism (actual execution of multiple tasks simultaneously on multiple cores)[cite: 10466, 12148, 16525, 18405].

---
## ⚙️ Fundamentals of Operating Systems (OS)
* [cite_start]**Role of the OS:** Viewed as "Arbiter" (manages resources), "Illusionist" (abstracts hardware), and "Glue" (provides common services)[cite: 114, 115, 116, 3175, 3178, 3180, 12192, 12194, 12196].
* [cite_start]**Kernel Abstractions:** The OS manages main abstractions such as Processes, Threads, Address Space, Files, and Sockets[cite: 149, 150, 151, 153, 3205, 3206, 3207, 3209, 10488, 10489, 10491, 10492, 12207, 12208, 12210, 12211, 12212, 17629, 17630, 17631, 17633, 18423, 18424, 18426, 18427, 18428, 19218, 19219, 19227, 19228].
* [cite_start]**Operation Modes:** Differentiation between **User Mode** (where applications run) and **Kernel Mode** (where the OS core runs with privileges), ensuring protection[cite: 1600, 1622, 1623, 3953, 3972, 3973, 12608, 12630, 12631].
* [cite_start]**System Calls:** Interface that allows a user-mode application to request kernel services (e.g., `read`, `write`, `fork`)[cite: 126, 405, 4009, 12615, 12638, 12893].

---
## 🏃 Processes and Threads
* [cite_start]**Process:** A program in execution with its own protected address space (memory)[cite: 1467, 3349, 10482, 12276, 13024, 18439, 18590, 19210].
* [cite_start]**Memory Structure:** A process’s address space is divided into `text`, `data` (initialized and uninitialized), `heap` (dynamic allocation), and `stack` (function call stack)[cite: 3377, 3378, 3379, 3380, 12289, 12290, 12291, 12292, 12309, 12311, 12315, 12325, 18455, 18457, 18458, 18461, 18463, 18464].
* [cite_start]**Process States:** The lifecycle of a process, including `new`, `ready`, `running`, `waiting`, and `terminated`[cite: 1928, 13054, 13055, 13056, 18510, 18511, 18512, 18514, 18515, 19251, 19252, 19253, 19255, 19256, 20734, 20737, 20738, 20740, 20741, 20743, 20744].
* [cite_start]**Process Control Block (PCB):** The kernel data structure that stores the entire context of a process (PID, registers, state, etc.)[cite: 2048, 2060, 3875, 13174, 13183, 18501, 18502, 18503, 18504, 19242, 19243, 19244, 19245, 20746].
* **Process Creation (POSIX):**
    * [cite_start]`fork()`: Creates an exact copy of the current process (child process)[cite: 2170, 13778, 13813, 18538].
    * [cite_start]`exec()`: Replaces the current process image with a new program[cite: 2171, 13779, 13814, 18539].
    * [cite_start]`wait()`: The parent process waits for the child process to terminate[cite: 2172, 13780, 13815, 18540].
* **Thread:** A basic unit of execution (control flow) *within* a process. [cite_start]Threads of the same process share the address space (memory `code`, `data`, `heap`), but each has its own stack (`stack`), program counter (PC), and registers[cite: 1464, 2407, 2433, 2478, 2480, 3346, 3569, 10609, 10665, 12273, 12499, 14231, 18436, 18690, 18716, 18763, 19318, 20670].
* **Thread APIs:**
    * [cite_start]**POSIX (pthreads):** `pthread_create()`, `pthread_join()`, `pthread_exit()`[cite: 2563, 2582, 18839, 18847, 18867, 18877].
    * [cite_start]**C++11:** `std::thread`, `join()`, `detach()`[cite: 18828, 19058, 19080, 19086, 19109, 19126].

---
## 🔒 Synchronization
* [cite_start]**Race Condition:** A bug where the result of a computation depends on the unpredictable order in which multiple threads access and modify shared data[cite: 4910, 10786, 10903, 10953, 11003, 14353, 19440]. [cite_start]The Therac-25 radiation therapy machine is a tragic example[cite: 11013, 11016].
* [cite_start]**Critical Section:** The code block that accesses shared resources and must be protected to ensure mutual exclusion (only one thread at a time)[cite: 4921, 4951, 11096, 11126, 14364, 14394, 19451].
* [cite_start]**Hardware Solutions:** CPU atomic instructions, such as **Test&Set** [cite: 5023, 11441, 14466] [cite_start]and **Compare&Swap** [cite: 5039, 11475, 14482][cite_start], which form the basis for *spinlocks*[cite: 5055, 11518, 14498].
* **Mutex (Mutual Exclusion):** The primary synchronization mechanism for protecting a critical section.
    * [cite_start]**Operations:** `lock()` (locks) and `unlock()` (unlocks)[cite: 5089, 5088, 11592, 11593, 14531, 14532, 19618, 19619].
    * [cite_start]**APIs:** `pthread_mutex_t` (POSIX) [cite: 11622, 14559] [cite_start]and `std::mutex` (C++)[cite: 5161, 11774, 14603].
    * [cite_start]**RAII (Resource Acquisition Is Initialization):** A C++ pattern (`std::lock_guard`, `std::unique_lock`) that ensures a mutex is automatically unlocked when leaving scope, preventing errors[cite: 5250, 11862, 11900, 11962, 14692, 19779].
* **Semaphores:** A synchronization object (created by Dijkstra) that uses an internal counter.
    * [cite_start]**Operations:** `wait()` (P or *down*) decrements the counter (and blocks if zero) and `signal()` (V or *up*) increments the counter (and wakes a blocked thread)[cite: 5445, 5446, 14733, 14734, 19820, 19821].
    * [cite_start]**Types:** **Binary** (value 0 or 1, acts like a mutex) [cite: 5638, 14926, 20013] [cite_start]and **Counting** (value 0 to N, manages a resource *pool*)[cite: 5639, 14927, 20014].
    * [cite_start]**APIs:** `sem_t` (POSIX) [cite: 15005] [cite_start]and `std::counting_semaphore` (C++20)[cite: 15091, 15108].
* **Condition Variables:** Used *together* with a mutex, allowing threads to wait (releasing the mutex) until a specific condition becomes true.
    * [cite_start]**Operations:** `wait()` (waits for the condition, atomically releasing the mutex), `notify_one()` (wakes one waiting thread), `notify_all()` (wakes all)[cite: 6193, 6194, 6195, 7989, 7990, 7991].
    * [cite_start]**APIs:** `pthread_cond_t` (POSIX) [cite: 6241] [cite_start]and `std::condition_variable` (C++)[cite: 6211].
* **Classic Problems:** Scenarios used to test synchronization mechanisms:
    * [cite_start]**Producers and Consumers:** Management of a shared buffer[cite: 5316, 5842, 6024, 8062, 15131, 15182, 20218, 20269].
    * [cite_start]**Readers and Writers:** Allows multiple simultaneous readers or a single writer[cite: 6392, 8290, 15414, 20501].
    * [cite_start]**Dining Philosophers:** Classic example to illustrate *deadlock*[cite: 8057, 8498, 8531].
    * [cite_start]**Sleeping Barber** [cite: 8058, 8932] [cite_start]and **Dining Savages**[cite: 8059, 9093].
* [cite_start]**Deadlock:** Situation where two or more tasks mutually block each other, each waiting for a resource held by the other[cite: 8629, 8737, 8738, 8739, 8740, 8898, 8899, 8900, 8901, 8902].

---
## 📡 Communication (I/O and IPC)
* [cite_start]**I/O Hardware (Input and Output):** Components such as controllers, buses (e.g., PCIe), and ports[cite: 229, 566, 581, 584, 586].
* [cite_start]**I/O Software:** The OS abstracts hardware through *Device Drivers*[cite: 230, 976, 988, 1003, 1101, 1103, 16968, 16996].
* **I/O Addressing:**
    * [cite_start]**Port-Mapped I/O:** Devices use a separate "port" address space (e.g., Intel `IN`/`OUT` instructions)[cite: 730, 745, 12361, 16912, 19548].
    * [cite_start]**Memory-Mapped I/O (MMIO):** Devices are mapped into the RAM address space[cite: 748, 756, 778, 12362, 16916, 16938, 19550]. [cite_start]`mmap()` is the *system call* that maps files into memory[cite: 792, 9583, 16952].
* **I/O Mechanisms:**
    * [cite_start]**Polling (Busy Waiting):** The CPU repeatedly checks the device status[cite: 1139, 1150, 1165, 16958, 17708].
    * [cite_start]**Interrupts:** The device asynchronously notifies the CPU when ready[cite: 878, 882, 912, 16963, 17713].
    * [cite_start]**DMA (Direct Memory Access):** A special controller transfers data between the device and RAM without involving the CPU[cite: 1233, 1254, 1268].
* **Timers:**
    * [cite_start]Hardware (e.g., HPET) and clocks (e.g., `CLOCK_MONOTONIC`) are used to measure time[cite: 17024, 17036, 17053, 17055].
(Content truncated due to size limit. Use page ranges or line ranges to read remaining content)
