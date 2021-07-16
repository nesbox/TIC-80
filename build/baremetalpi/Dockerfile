FROM ubuntu:18.04
RUN apt-get update
RUN apt-get install software-properties-common -y
RUN add-apt-repository ppa:git-core/ppa -y
RUN apt-get install wget git build-essential -y
RUN wget -q https://cmake.org/files/v3.13/cmake-3.13.0-Linux-x86_64.sh
RUN sh cmake-3.13.0-Linux-x86_64.sh --skip-license --prefix=/usr
RUN cd /opt && wget -q https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/9-2020q2/gcc-arm-none-eabi-9-2020-q2-update-x86_64-linux.tar.bz2
RUN cd /opt && tar xjf gcc-arm-none-eabi-9-2020-q2-update-x86_64-linux.tar.bz2
ENV PATH=/opt/gcc-arm-none-eabi-9-2020-q2-update/bin/:$PATH
RUN mkdir /src
WORKDIR /src
