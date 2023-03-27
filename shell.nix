{ pkgs ? import <nixpkgs> { config.allowUnfree = true; } }:

let
  pythonEnv = pkgs.python310.withPackages (p: with p; [
    datasets
    librosa
    numpy
    pytorchWithCuda
    soundfile
    transformers
  ]);

  venvDir = "./.venv";

  createVenv = ''
    if [ ! -d "${venvDir}" ]; then
      ${pythonEnv.interpreter} -m venv --system-site-packages ${venvDir}
    fi
  '';

  activateVenv = ''
    . ${venvDir}/bin/activate
  '';

  installPythonPackages = ''
    pip install jiwer evaluate
  '';
in
(pkgs.mkShell.override { stdenv = pkgs.llvmPackages_15.stdenv; }) {
  buildInputs = with pkgs; [
    eigen
    ffmpeg
    onnxruntime
    openblas
    pkgsStatic.boost
    pkgsStatic.openssl
    pkgsStatic.protobuf
    pkgsStatic.websocketpp
  ];

  packages = with pkgs; [
    clang-tools
    cmake
    cudaPackages.cudatoolkit
    cudaPackages.cudnn
    ninja
    pkg-config
    python310Packages.pip
    python310Packages.virtualenv
    pythonEnv
  ];

  shellHook = createVenv + activateVenv + installPythonPackages;
}

