FROM ubuntu:18.04
RUN apt-get update
RUN apt-get install software-properties-common -y
RUN add-apt-repository ppa:git-core/ppa -y
RUN apt-get install wget git curl xz-utils pkg-config libarchive13 -y
RUN wget https://github.com/devkitPro/pacman/releases/download/v1.0.2/devkitpro-pacman.amd64.deb
RUN dpkg -i devkitpro-pacman.amd64.deb
RUN dkp-pacman --noconfirm --sync devkitARM devkitpro-pkgbuild-helpers 3ds-libpng 3ds-pkg-config 3ds-zlib 3dstools libctru citro3d
ENV DEVKITPRO=/opt/devkitpro
RUN wget https://cmake.org/files/v3.13/cmake-3.13.0-Linux-x86_64.sh
RUN sh cmake-3.13.0-Linux-x86_64.sh --skip-license --prefix=/usr
RUN mkdir /src
WORKDIR /src
