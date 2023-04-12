{
  description = "TruePrompter";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem
      (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        rec {
          packages.trueprompter = import ./default.nix { inherit pkgs; };
          packages.default = packages.trueprompter;
          apps.server = {
            type = "app";
            program = "${packages.trueprompter}/bin/trueprompter_server";
          };
          apps.client = {
            type = "app";
            program = "${packages.trueprompter}/bin/trueprompter_client";
          };
          packages.dockerImage = pkgs.dockerTools.buildImage {
            name = "trueprompter";
            tag = "latest";
            config = { Cmd = [ apps.server.program ]; };
          };
          devShells.default = (
            pkgs.mkShell.override {
              stdenv = packages.trueprompter.stdenv; }
          ) {
            packages = packages.trueprompter.nativeBuildInputs;
            buildInputs = packages.trueprompter.buildInputs;
          };
        }
      );
}

