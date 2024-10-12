---
layout: layout.vto
---


This is a quick coding exercise to get handy with the codebase, toolchains, etc.  

## Step 1

Clone the tasks repo

```shell
git clone https://github.com/sidosera/getrafty.git && cd getrafty/tasks/hello-world
```


## Step 2

Launch dev container

```shell
clippy boot --build 
```

The container is now running on port `3333` and freely accessible under `ubuntu` user. To continue in terminal login via `ssh ubuntu@localhost -p 3333` or simply `clippy attach`. It's recommended to configure an IDE of your choice.


## Step 3 

Your goal is to complete the method `computeTimeTravelSpeed` in class `Delorean` declared in file `time_machine.hpp`. 

```c++
// time_machine.cpp

uint32_t Delorean::computeTimeTravelSpeed() const {
    // Your code goes here
};
```

The method should return the speed measured in mph of a Delorean time machine [to be able to travel in time](https://en.wikipedia.org/wiki/DeLorean_time_machine#Operation).

##  Step 4

Ensure your code pass clang-tidy lints. Every task comes with tests, and you should be able to run it via [CMake](https://cmake.org/cmake/help/latest/guide/tutorial/A%20Basic%20Starting%20Point.html).   


Eventually this step should be automated and task solution could be checked via tasklet:

Run `clippy test` inside `~/workspace/tasks/hello-world/`

```shell
cd ~/workspace/tasks/hello-world & clippy test
```

Have questions? Check [Q&A](/etc/faq/) for most common issues or file an issue on the [GitHub](https://github.com/sidosera/getrafty/issues). 


