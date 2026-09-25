FROM gcc:12.5 AS build

RUN apt-get update && \
    apt-get install -y --no-install-recommends python3-pip cmake && \
    pip3 install --no-cache-dir conan==1.62.0 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY conanfile.txt .
RUN mkdir build && cd build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release

COPY CMakeLists.txt .
COPY src ./src
RUN cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . -j$(nproc)

FROM ubuntu:22.04 AS runtime

RUN groupadd -r www && useradd -r -g www www
WORKDIR /app

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

USER www
EXPOSE 8080
ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static"]
