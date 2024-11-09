---
layout: layout.vto
---


# Runtime

This is an introductory task where we touch on the concept of thread pool, the core component for scheduling and executing work in our distributed file system. [Backend systems communicate by sending and receiving messages](https://en.wikipedia.org/wiki/Distributed_computing), upon receiving a message the system typically runs some work. Modern hardware trending to [scale](https://homepages.inf.ed.ac.uk/bgrot/pubs/SOP_ISCA12.pdf) much more through support of multiple cores rather than growing frequency of individual computation units. 


![Scaling law](/static/img/scaling-cpu.png)

*~ Scaling trends for the transistor count, clock frequency, number of cores, and single-thread performance of microprocessor chips - [ResearchGate](https://www.researchgate.net/figure/Scaling-trends-for-the-transistor-count-clock-frequency-number-of-cores-and_fig2_224168227)*

To utilize the full capacity of the hardware we will run those callbacks on multiple cores / threads by scheduling them on the thread pool. For further task the thread pool will serve as a main infrastructure component for scheduling and executing work in parallel.

### Just pool it


A thread pool is a programming concept that pre-allocates a set of reusable worker threads to execute tasks. Instead of creating a new thread for every task, the thread pool manages a queue of incoming tasks and assigns them to available threads, avoiding the overhead of frequent thread creation. Thread creation is a resource-intensive operation, consuming time and memory, especially when dealing with a high volume of tasks. By reusing threads, the thread pool reduces these costs and ensures efficient use of CPU and memory resources. 

Common implementations of thread pools include JVM's [ExecutorService](https://docs.oracle.com/en/java/javase/22/docs/api/java.base/java/util/concurrent/ExecutorService.html), Python's [concurrent.futures.ThreadPoolExecutor](https://docs.python.org/3/library/concurrent.futures.html#concurrent.futures.ThreadPoolExecutor), and C++'s [Folly executors](https://github.com/facebook/folly/blob/main/folly/docs/Executors.md).

There are multiple scheduling algorithms to organize work in the thread pool such as FIFO, [Work Stealing](https://en.wikipedia.org/wiki/Work_stealing), etc. In this task you're suggested to implement the straightforward approach with a single shared queue for distributing load across multiple threads.

![Basic thread pool](/static/img/basic-thread-pool.png)
*~ A sample thread pool (green boxes) with waiting tasks (blue) and completed tasks (yellow) - [Wiki](https://en.wikipedia.org/wiki/Thread_pool)*


### Interface

The main method of the thread pool is `submit` which adds the task to the queue, and returns control back to the caller.  Methods `start` and `stop` are there for managing lifecycle of the pool and self-explanatory:

```c++
using Task = std::function<void()>;

// Fixed-size pool of worker threads

class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    
    void start();
    bool submit(Task&&);
    void stop();
    
}
```


### Queue

Our thread pool implementation will rely on a shared queue. Callers can add tasks to the queue using the `submit` method, while worker threads monitor the queue and pick up new tasks once they complete their current ones. But what happens when there’s no work available for a thread?

There are several strategies to handle this. One approach is to continuously retry, checking the queue until work appears. However, this can be inefficient, as it wastes CPU cycles on idle polling without providing any value. A more efficient alternative is to "park" the thread, taking it off the CPU until new tasks are available.

```c++
// Unbounded blocking multi-producers/multi-consumers (MPMC) queue
template<typename T> 
class UnboundedBlockingQueue {
public:
    void put(T v);
    T take(); // This will block if queue is empty
}
```

The thread pool will rely on `UnboundedBlockingQueue`. You're given a naive implementation of this queue with `take` implementing a busy wait strategy, replace it with appropriate wait strategy without burning CPU cycles.


### Testing


Tests are located in `thread_pool_test.cpp`

Have questions? Check [Q&A](/etc/faq/) for most common issues or file an issue on the [GitHub](https://github.com/sidosera/getrafty/issues). 

