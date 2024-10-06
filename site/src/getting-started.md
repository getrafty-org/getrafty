---
layout: layout.vto
---

# Getting Started


> /**
>  *The only officially supported way to run this class labs it though Docker. Although, you can set up the dev environment on you host machine, this approach hasn't been tested.*
> **/


### Get the repository

Clone the git repository which contains the contents of the class.


```bash
cd ~/my-projects
git clone https://github.com/sidosera/getrafty.git && cd getrafty
```

This class repository is shipped with an assistant tool called Clippy. Clippy is a simple shell script that helps to automate many frequent activities such as testing solutions or connecting to the dev VM.
Check if Clippy works as expected.
```bash
chmod +x clippy.sh # to make things easier, we can make this file executable.

./clippy.sh
```

It should print the regular command help message.


### Login to dev container

The repository includes dev container packing necessary dependencies and provides a straightforward way to get started.

This command will run a new instance of dev container. 

```
./clippy.sh boostrap --build
```


If everything is good, the container should be running with an open ssh port `3333` and user `ubuntu`. Connect using SSH or Clippy:

```
# Connect via SSH
ssh ubuntu@localhost -p 3333
```

```
# Connect via Clippy
./clippy.sh attach
```

### Jump to the fist exercise

You're ready for the first exercise [hello-world](/hello-world) ;)

