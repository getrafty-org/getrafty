#!/bin/sh

export PROJECT_NAME=getrafty
export PROJECT_DIR=$(pwd)
export SSH_BIND="$(hostname -I | awk '{print $1}'):3333"

docker-compose up -d
docker exec $PROJECT_NAME /bin/sh -c "adduser --disabled-password --gecos '' me"
docker exec $PROJECT_NAME /bin/sh -c "usermod -a -G sudo me"
docker exec $PROJECT_NAME /bin/sh -c "echo 'me\nme' | passwd me"