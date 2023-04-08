{ pkgs ? import <nixpkgs> { config.allowUnfree = true; } }:

(pkgs.mkShell.override { stdenv = pkgs.llvmPackages_15.stdenv; }) {
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

  packages = with pkgs; [
    clang-tools
    cmake
    ninja
    pkg-config
  ];
}

