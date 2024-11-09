---
layout: layout.vto
---


# Let's Get Rafty

This project is a series of write-ups and exercises on distributed systems with a goal to build a curated list of exercise for hackers who enjoy building things. 

Please keep your repository private when forking, and avoid sharing solutions publicly. Thanks ! ❤️


![Scenes from distributed systems](/static/img/scenes-from-distributed-systems.webp)

*~ [scenes from distributed systems by Julia Evans, X (Twitter)](https://x.com/b0rk/status/1056560207562711041)* 

## What

This is the project with an idea to feature a range of write-ups and exercises on distributed systems for beginners. It supposed to offer a grasp overview of the space. If you've ever been curious about how [distributed file systems](https://static.googleusercontent.com/media/research.google.com/en//archive/gfs-sosp2003.pdf) function or wanted to explore the inner workings of [your favorite RPC framework](https://en.wikipedia.org/wiki/Apache_Thrift). Then, it's the right place here because you will build a fully functioning toy version of that yourself.

The exercises are mostly isolated units of work covering a particular topic. Each of the exercise comes with boilerplate implementation in C++ which you need to adjust. 

Get ready 🍿


## Catch

[Why do we need distributed systems anyway?](https://brooker.co.za/blog/2020/01/02/why-distributed.html)

## Navigation 🗺️

- [How to work with this repository](getting-started)
    - [Tutorial](hello-world)
- Index
  - [Remote Procedure Calls @I. Thread pool](/thread-pool) 
  - [Remote Procedure Calls @II. IO](/rpc-io)  
  - 🚧 [Remote Procedure Calls @III. Calling functions](#)
  - 🚧 [Filesystem @I.Accessing remotely over RPC](#)
  - 🚧 [Filesystem @II. Surviving node failures](#)

- Misc.
    - [Install Docker](etc/use-docker)
    - [SSH to dev container](etc/connect-over-ssh)
    - [How to use this repository with VSCode](etc/use-vscode)

- [FAQ](etc/faq)

## Contributing

This is a very early exploration that is yet to be proved to be useful; the contents are very immature and incomplete.  If you want to contribute, **you are very welcome**! [File an issue](https://github.com/sidosera/getrafty/issues/new) and we will chat.  
If you have found a bug, please create an issue (or pull request) [in the repository](https://github.com/sidosera/getrafty).


## License

The source and documentation are released under the [MIT License](https://github.com/sidosera/getrafty/blob/main/LICENSE).

---
*Why the name is "Getrafty"? That appeared to me as a funny game of words. One of the crucial concept touched in the tutorials the [Raft Consensus Algorithm](https://raft.github.io/)*.



