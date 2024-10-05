---
layout: layout.vto
---

# Install Docker


### Linux

```bash
sudo apt install -y docker docker-compose bash
sudo groupadd docker
sudo usermod -aG docker $USER
```

Once installed, reload the current session (or simply reboot a machine).


### OS X

Download, install and launch [Docker Desktop](https://docs.docker.com/desktop/install/mac-install/). Make sure the toolbar now has a Docker icon.

### Check Docker has been installed

This should output an hello message from Docker

```bash
docker run hello-world
```