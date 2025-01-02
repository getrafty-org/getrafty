---
layout: layout.vto
---


Quick coding exercise to get handy with the codebase, toolchains, etc.  

## #1. Getting The Source Code

Clone the [git repository](https://github.com/getrafty-org/getrafty):

```shell
git clone git@github.com:getrafty-org/getrafty.git
cd getrafty/tasks/tutorial
```


## #2. Setting Up Dev Environment

Launch dev container with configured development environment

```shell
./tasklet.py boot 
```

The container is now running on port `3333` and accessible under `ubuntu` user. To continue in terminal login via `ssh ubuntu@localhost -p 3333` or simply `./tasklet.py attach`. It's recommended to configure an IDE of your choice.


## #3. Coding Solution

Your goal is to complete the method `FluxCapacitor::computeTimeBreakBarrierSpeed` in `flux_capacitor.cpp`. 

```c++
// In flux_capacitor.cpp:
Speed FluxCapacitor::computeTimeBreakBarrierSpeed() {
    ...
};
```

The method should return a minimum speed (measured in miles per hour) for a Delorean [to be able to travel in time](https://en.wikipedia.org/wiki/DeLorean_time_machine#Operation).

##  #4. Verifying Solution

Ensure your code passes clang-tidy lints. Every task comes with tests, and you should be able to run it via [CMake](https://cmake.org/cmake/help/latest/guide/tutorial/A%20Basic%20Starting%20Point.html).   


Note: Eventually this step should be automated and task solution could be checked via tasklet.


Have questions? Check [Q&A](/etc/faq/) for most common issues or file an issue on the [GitHub](https://github.com/getrafty-org/getrafty/issues). 


