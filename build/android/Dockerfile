FROM mingc/android-build-box
RUN apt-get update
RUN apt-get remove git -y
RUN apt-get install software-properties-common -y
RUN add-apt-repository ppa:git-core/ppa -y
RUN apt-get install git -y
RUN for ARCH in arm arm64 x86 x86_64; do API=24; SYSROOT=$ANDROID_NDK_HOME/platforms/android-$API/arch-$ARCH; python3 "$ANDROID_NDK_HOME/build/tools/make_standalone_toolchain.py" --api "$API" --arch "$ARCH" --install-dir "$SYSROOT" --force ;  ln -Ffs "$SYSROOT/sysroot/usr" "$SYSROOT/usr" ; done
RUN mkdir -p /usr/local/opt
RUN ln -Ffs "$ANDROID_NDK_HOME" /usr/local/opt/android-ndk
RUN mkdir /src
WORKDIR /src
