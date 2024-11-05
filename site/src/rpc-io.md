---
layout: layout.vto
---


# Remote Procedure Calls | IO

[Fallacies of distributed computing](https://en.wikipedia.org/wiki/Fallacies_of_distributed_computing)

We begin exploration of the world of distributes systems from the concept of RPC. RPC is a design paradigm that allow two entities to communicate over a communication channel in a general request-response mechanism. RPC creates an abstraction connecting caller and callee hiding the complexity of faulty network under the hood.

[RPC is Not Dead: Rise, Fall and the Rise of Remote Procedure Calls](http://dist-prog-book.com/chapter/1/rpc.html)

At its heart RPC deals a lot with IO, reading and writing data back to [sockets](https://en.wikipedia.org/wiki/Berkeley_sockets). The goal of this task is to implement that fundamental part of the RPC framework to let it interact with the system sockets.

Socket is a programming interface that operating system exposes for interacting with the network card. The concept of sockets exists in any widely used operating system, e.g. [POSIX](https://en.wikipedia.org/wiki/POSIX) specifies [Berkeley sockets](https://en.wikipedia.org/wiki/Berkeley_sockets) interface.

Berkeley sockets can operate in one of two modes: blocking or non-blocking. The non-blocking mode is generally preferable since it allows a single thread to handle multiple connections efficiently without getting stuck waiting for data. This is particularly valuable when dealing with many simultaneous connections, as it avoids the overhead of creating multiple threads.

You're given a `EventWatcher` class that allows to `watch` on a file descriptor and call a `IWatchCallback` callback whenever file descriptor becomes ready to be read from or written to.  

```c++
class EventWatcher {
public:
    void watch(int fd, WatchFlag flag, IWatchCallback *ch);

    void unwatch(int fd);

    void unwatchAll();

    static EventWatcher &getInstance();
    
private:
    // Main loop for waiting on and processing events
    void waitLoop();
};
```

The callback is a user defined structure responsible for reading / writing fd.

```c++
class IWatchCallback {
public:
    virtual void onReadReady(int fd) = 0;

    virtual void onWriteReady(int fd) = 0;

    virtual ~IWatchCallback() = default;
};
```

Your task is to implement `waitLoop` method of the `EventWatcher` class using [`epoll_wait`](https://man7.org/linux/man-pages/man2/epoll_wait.2.html#top_of_page).


### Cancellation

It's possible the `epoll_wait` will hang forever unless data appears in the fd being watched. This may be not desirable because it prevents newly added fd from being watched 'immediately' rather than after the current loop cycle.  Consider using [self pipe trick](https://cr.yp.to/docs/selfpipe.html) or timeout for epoll wait. Which one is better and why?

### Synchronisation

The epoll can detect fd being read / written in the callback as ready. This can result in data race unless proper synchronization is used inside user-defined callback structure. Think about ways to guarantee the callback are mutually exclusive for the same fd without requiring user-defined callback to synchronise access to fd.     

### Testing 

Tests are located in `event_watcher_test.cpp`











