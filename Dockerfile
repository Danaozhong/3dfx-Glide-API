FROM ubuntu:25.04

LABEL maintainer="Clemens Zangl" \
      description="Docker file to build 3dfx Glide API" \
      version="0.1"

# set timezone to make sure the install tzdata is successful
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moscow
# Make sure the image is updated, install some prerequisites,
RUN apt-get update -y

# Install GCC, Git (to checkout google test), and other helpful tools
RUN apt-get install -y --no-install-recommends \
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
    ca-certificates

RUN apt-get autoclean 
RUN apt-get autoremove 
RUN apt-get clean 
RUN rm -rf /var/lib/apt/lists/*


# Start from a Bash prompt
WORKDIR /home/workspace/
CMD /bin/bash