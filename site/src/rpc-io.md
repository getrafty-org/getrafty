---
layout: layout.vto
---


# Remote Procedure Calls | IO


We begin exploration of the world of distributes systems from the concept of RPC. RPC is a design paradigm that allow two entities to communicate over a communication channel in a general request-response mechanism. RPC creates an abstraction connecting caller and callee hiding the complexity of faulty network under the hood. There are plenty of RPC implementations such as [gRPC](https://grpc.io/), [Apache Thrift](https://thrift.apache.org/) or [Cap'n Proto](https://capnproto.org/).

🍿 [RPC is Not Dead: Rise, Fall and the Rise of Remote Procedure Calls](http://dist-prog-book.com/chapter/1/rpc.html)

Internally RPC deals a lot with IO, reading from and writing data back to [sockets](https://en.wikipedia.org/wiki/Berkeley_sockets). In this task you will implement this fundamental part of any RPC framework.

Socket is a programming interface operating system exposes to interacting with the network card. The concept of sockets exists in any widely used operating system, e.g. [POSIX](https://en.wikipedia.org/wiki/POSIX) specifies [Berkeley sockets](https://en.wikipedia.org/wiki/Berkeley_sockets) interface.


In Linux (using Berkeley sockets), sockets can work in two modes: blocking and non-blocking. In blocking mode, a thread waits until data is available, which can lead to wasted CPU time. Instead, we will use non-blocking mode, where threads don’t have to sit idle while waiting for data, it can do something different before  operating system notifies it. This approach helps make better use of threads and system resources. We refer to this method as asynchronous I/O.

The asynchronous IO is done using the concept of multiplexing implemented via OS mechanisms like [`epoll`](https://man7.org/linux/man-pages/man7/epoll.7.html) or higher level mechanisms like event loop, e.g. [`libuv`](https://github.com/libuv/libuv).    

### Interface

For this task, we won’t be creating a fully functional event loop. Instead, we’ll concentrate on a specific aspect of its functionality: the notification mechanism. You're given a `EventWatcher` class implementing this mechanism. It allows to `watch` on a file descriptor and call a `IWatchCallback` callback whenever file descriptor becomes ready for reading or writing.

```c++
class EventWatcher {
public:
    void watch(int fd, WatchFlag flag, IWatchCallback *ch);
    void unwatch(int fd);
    void unwatchAll();
    static EventWatcher &getInstance();
private:
    // Loop for waiting for fb to become ready for reading or writing 
    // and scheduling callbacks 
    void waitLoop();
};
```

The callback is called once fd (e.g. socket) becomes ready. 

```c++
class IWatchCallback {
public:
    virtual void onReadReady(int fd) = 0;
    virtual void onWriteReady(int fd) = 0;
};
```

Your task is to implement `waitLoop` method of the `EventWatcher` class using [`epoll_wait`](https://man7.org/linux/man-pages/man2/epoll_wait.2.html#top_of_page).

### Where to schedule callbacks?

If a user-defined callback is scheduled on the same thread as the `waitLoop`, it can interfere with the loop logic impacting the overall performance. To address this, the `EventWatcher` allows specifying a thread pool in its constructor, which is used to run the main loop. This thread pool can also be utilized to execute user-provided callbacks.

### Cancellation mechanism

It's possible the `epoll_wait` will hang forever unless data appears in the fd being watched. This may be not desirable because it prevents newly added fd from being watched 'immediately' rather than after the current loop cycle.  Consider using [self pipe trick](https://cr.yp.to/docs/selfpipe.html) or timeout for epoll wait. *Which one is better and why?*

### Synchronisation

The epoll can detect fd being read / written in the callback as ready. This can result in data race unless proper synchronization is used inside user-defined callback structure. Think about ways to guarantee the callback are mutually exclusive for the same fd without requiring user-defined callback to synchronise access to fd.     

### Testing 

Tests are located in `event_watcher_test.cpp`.

Have questions? Check [Q&A](/etc/faq/) for most common issues or file an issue on the [GitHub](https://github.com/sidosera/getrafty/issues). 










