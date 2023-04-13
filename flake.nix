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
            copyToRoot = pkgs.buildEnv {
              name = "root";
              paths = [
                packages.trueprompter
                ./.
              ];
              pathsToLink = [ "/bin" "/model" ];
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

