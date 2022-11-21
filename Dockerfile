FROM ubuntu:22.04
ENV DEBIAN_FRONTEND noninteractive
RUN apt update
RUN apt install -y git clang ninja-build cmake pkg-config  \
    libopenblas-dev libboost-all-dev libssl-dev libwebsocketpp-dev protobuf-compiler  \
    libavcodec-dev libavformat-dev libavdevice-dev libavutil-dev libavfilter-dev libswscale-dev libpostproc-dev libswresample-dev
COPY . /app/src
RUN mkdir /app/bin
RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_GENERATOR=Ninja -S /app/src -B /app/bin
RUN cmake --build /app/bin --target trueprompter_server --parallel
WORKDIR /app
CMD ["bin/trueprompter/server/trueprompter_server", "8080", "src/small_model"]
EXPOSE 8080/tcp
