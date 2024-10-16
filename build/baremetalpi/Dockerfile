FROM ubuntu:24.04
RUN apt-get update
RUN apt-get install software-properties-common -y
RUN add-apt-repository ppa:git-core/ppa -y
RUN apt-get install wget git build-essential -y
RUN wget -q https://cmake.org/files/v3.30/cmake-3.30.3-linux-x86_64.sh
RUN sh cmake-3.30.3-linux-x86_64.sh --skip-license --prefix=/usr
RUN cd /opt && wget -q https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi.tar.xz
RUN cd /opt && tar -xf arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi.tar.xz
ENV PATH=/opt/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin/:$PATH
RUN mkdir /src
WORKDIR /src
