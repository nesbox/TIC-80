FROM ubuntu:18.04
RUN apt-get update && apt-get install -y git cmake libgtk-3-dev \ 
    libglvnd-dev libglu1-mesa-dev freeglut3-dev libasound2-dev
WORKDIR ${DAPPER_SOURCE:-/source}
ENV DAPPER_OUTPUT bin
ENTRYPOINT ["sh", "-c", "cmake . && make -j4"]
