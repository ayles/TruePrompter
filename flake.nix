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
            fromImage = pkgs.dockerTools.pullImage {
              imageName = "ghcr.io/ayles/truepromptermodel";
              imageDigest = "sha256:5e9c34d2b4f033759f09e18279a4436caed34d6e664dc20d84be61ac7cb8c375";
              sha256 = "sha256-p5t/fIyVZNxjYwayeEp1MUc5VH+rpgJTnF5zPB0BJwQ=";
              finalImageTag = "test";
              finalImageName = "truepromptermodel";
            };
            name = "trueprompter";
            tag = "latest";
            copyToRoot = pkgs.buildEnv {
              name = "root";
              paths = [
                packages.trueprompter
              ];
              pathsToLink = [ "/bin" ];
            };
            config =
            let
              port = "8080";
            in {
              Cmd = [ apps.server.program port "/model" "/logs/info.log" "/logs/debug.log" ];
              ExposedPorts = {
                "${port}/tcp" = {};
              };
            };
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

