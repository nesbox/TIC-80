FROM ubuntu:18.04
RUN apt-get update
RUN apt-get install software-properties-common -y
RUN add-apt-repository ppa:git-core/ppa -y
RUN apt-get install wget git make -y
RUN mkdir /opt/sysroot
RUN cd /opt/sysroot && wget -q http://downloads.raspberrypi.org/raspbian/root.tar.xz && tar xf root.tar.xz
RUN cd /opt/sysroot && wget -q https://buildbot.libsdl.org/sdl-builds/sdl-raspberrypi/sdl-raspberrypi-224.tar.xz && tar xf sdl-raspberrypi-224.tar.xz
RUN cd /opt && git clone --depth 1 https://github.com/raspberrypi/tools.git tools
RUN wget -q https://cmake.org/files/v3.13/cmake-3.13.0-Linux-x86_64.sh
RUN sh cmake-3.13.0-Linux-x86_64.sh --skip-license --prefix=/usr
RUN mkdir /src
WORKDIR /src
