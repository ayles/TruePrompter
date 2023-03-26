{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    openblas
    ffmpeg
    pkgsStatic.openssl
    pkgsStatic.boost
    pkgsStatic.protobuf
    pkgsStatic.websocketpp
  ];

  packages = with pkgs; [
    clang
    cmake
    ninja
    pkg-config
    python3
  ];
}

