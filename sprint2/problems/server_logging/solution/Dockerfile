FROM gcc:11.3 AS build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.52.0

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release

COPY ./src /app/src
COPY ./data /app/data
COPY ./static /app/static
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . && \
    cp /app/build/bin/game_server /app/game_server

ENTRYPOINT ["/app/game_server", "/app/data/config.json", "/app/static/"]
