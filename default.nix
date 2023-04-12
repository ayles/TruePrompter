{ pkgs }:

pkgs.llvmPackages_15.stdenv.mkDerivation {
  pname = "trueprompter";
  version = "0.1.0";

  src = ./.;

  nativeBuildInputs = with pkgs; [
    clang-tools
    cmake
    ninja
    pkg-config
  ];

  buildInputs = with pkgs; [
    eigen
    ffmpeg
    onnxruntime
    openblas
    alsa-lib
    gnuplot
    pkgsStatic.boost
    pkgsStatic.openssl
    pkgsStatic.protobuf
    pkgsStatic.websocketpp
  ];

  configurePhase = ''
    mkdir build
    cmake -S ./ -B ./build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_GENERATOR=Ninja -DCMAKE_INSTALL_PREFIX=$out
  '';

  buildPhase = ''
    cmake --build ./build
  '';

  installPhase = ''
    cmake --install ./build
  '';
}

