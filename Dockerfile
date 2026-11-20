FROM ubuntu:26.04

LABEL maintainer="Clemens Zangl" \
      description="Docker file to build 3dfx Glide API" \
      version="0.2" \
      org.opencontainers.image.source="https://github.com/Danaozhong/3dfx-Glide-API"

# set timezone to make sure the install tzdata is successful
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moscow

# Install GCC, Git (to checkout google test), and other helpful tools. In one
# layer, so the package lists removed at the end are not kept in the image.
RUN apt-get update -y && \
    apt-get install -y --no-install-recommends \
    xz-utils \
    build-essential \
    curl \
    cmake \
    git \
    gcc \
    tar \
    nasm \
    gcc-multilib \
    g++-multilib \
    mingw-w64 \
    ca-certificates && \
    apt-get autoremove -y && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Start from a Bash prompt
WORKDIR /home/workspace/
CMD ["/bin/bash"]
