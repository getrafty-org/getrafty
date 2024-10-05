---
layout: layout.vto
---

# Getting Started


> /**
>  ☝️ *The only officially supported way to run this class labs it though Docker. Although, you can setup the dev environemnt on you host machine, this approach hasn't been tested.*
> **/


### Get the repository

Clone the git repository which containes the contents of the class.

```bash
git clone https://github.com/sidosera/getrafty.git
```

This class repository is shipped with an assitant tool called Clippy. Clippy is a simple shell script that helps to automate many frequent activities such as testing solutions or connecting to the devvm.


```bash
cd getrafty
chmod +x clippy.sh # to make things easier, we can make this file executable.
```

Check if Clippy works as expected.
```bash
./clippy.sh
```

This commads should print the usage page.

### Login to dev container

This command will run a fresh instance of dev container (devvm). 

```
./clippy.sh boostrap --build
```


If everything is good, we should be able to connect to our devvm via

```
./clippy.sh attach
```

### Fist exercise

@TODO

