# Unfortunately, alpine is too hard to get build working on
# Probably should try to build on ubuntu and run on alpine later
FROM ubuntu:22.04 as builder
ENV DEBIAN_FRONTEND noninteractive
RUN apt update
RUN apt install -y git clang ninja-build cmake pkg-config \
    libopenblas-dev libboost-all-dev libssl-dev libwebsocketpp-dev libprotobuf-dev protobuf-compiler \
    libavcodec-dev libavformat-dev libavdevice-dev libavutil-dev libavfilter-dev libswscale-dev libpostproc-dev libswresample-dev
COPY . /app/src
RUN mkdir /app/bin
RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_GENERATOR=Ninja -S /app/src -B /app/bin
RUN cmake --build /app/bin --target trueprompter_server --parallel

FROM ubuntu:22.04
ENV DEBIAN_FRONTEND noninteractive
RUN apt update
RUN apt install -y libopenblas-base ffmpeg
COPY --from=builder /app/bin/trueprompter/server/trueprompter_server /app/bin/trueprompter/server/trueprompter_server
COPY --from=builder /app/src/small_model /app/src/small_model
WORKDIR /app
CMD ["bin/trueprompter/server/trueprompter_server", "8080", "src/small_model"]
EXPOSE 8080/tcp