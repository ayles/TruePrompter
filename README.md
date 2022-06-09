## TruePrompter
Teleprompter server with speech recognition

## Build
```
sudo apt install libopenblas-dev libboost-all-dev libssl-dev libwebsocketpp-dev protobuf-compiler ninja-build
git clone https://github.com/ayles/TruePrompter.git
cd TruePrompter
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_GENERATOR=Ninja ..
cmake --build --target TruePrompter .
cd ..
```

NOTE: building without specified target will not work

## Usage
```
build/src/TruePrompter 8080 small_model
```
