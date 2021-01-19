FROM mingc/android-build-box
RUN apt-get update
RUN apt-get remove git -y
RUN apt-get install software-properties-common -y
RUN add-apt-repository ppa:git-core/ppa -y
RUN apt-get install git -y
RUN mkdir /src
WORKDIR /src
