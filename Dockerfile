# Base image
FROM ubuntu:24.04


# Set environment variables
ENV TZ=Europe/London \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    CXX="/usr/bin/clang++-17" \
    CC="/usr/bin/clang-17" 

# Dependency
ARG DEPS=" \
    libfuse3-3 \
    libfuse3-dev \
    ssh \
    make \
    cmake \
    build-essential \
    ninja-build \
    git \
    linux-tools-common \
    linux-tools-generic \
    g++-12 \
    clang-17 \
    clang-format-17 \
    clang-tidy-17 \
    libclang-rt-17-dev \
    libc++-17-dev \
    libc++abi-17-dev \
    clangd-17 \
    lldb-17 \
    gdb \
    binutils-dev \
    libdwarf-dev \
    libdw-dev \
    ca-certificates \
    openssh-server \
    vim \
    autoconf \
    curl \
    unzip \
"

RUN apt-get update -q && \
    apt-get install -y sudo && \
    echo "ubuntu ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
    passwd -d ubuntu

    
# Install dependencies
RUN apt-get update -q && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y tzdata software-properties-common wget rsync && \
    add-apt-repository -y ppa:ubuntu-toolchain-r/test && \
    add-apt-repository "deb http://archive.ubuntu.com/ubuntu focal main universe restricted multiverse" && \
    add-apt-repository universe && \
    wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor | tee /etc/apt/trusted.gpg.d/llvm-snapshot.gpg > /dev/null && \
    add-apt-repository -y "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-17 main" && \
    wget -qO- https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor | tee /etc/apt/trusted.gpg.d/kitware-archive.gpg > /dev/null && \
    add-apt-repository 'deb https://apt.kitware.com/ubuntu/ focal main' && \
    apt-get update -q && \
    apt-get install -y $DEPS



# Configure SSH daemon for no authentication
RUN sed -i 's/#PermitEmptyPasswords no/PermitEmptyPasswords yes/' /etc/ssh/sshd_config && \
    sed -i 's/#PasswordAuthentication yes/PasswordAuthentication yes/' /etc/ssh/sshd_config && \
    sed -i 's/#ChallengeResponseAuthentication yes/ChallengeResponseAuthentication no/' /etc/ssh/sshd_config && \
    echo "PermitRootLogin yes" >> /etc/ssh/sshd_config && \
    echo "PermitUserEnvironment yes" >> /etc/ssh/sshd_config

# Configure SSH
RUN echo "StrictHostKeyChecking=no" >> /etc/ssh/ssh_config && mkdir /var/run/sshd


# Clean up
RUN rm -rf /var/lib/apt/lists/*

EXPOSE 22 3000

# Start SSH service
CMD ["/usr/sbin/sshd", "-D"]