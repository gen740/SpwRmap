{
  description = "Flake shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs =
    inputs@{
      flake-parts,
      nixpkgs,
      ...
    }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = nixpkgs.lib.platforms.all;

      perSystem =
        { pkgs, ... }:
        let
          clangTools = pkgs.llvmPackages.clang-tools.override { enableLibcxx = true; };
        in
        rec {
          devShells.default = pkgs.mkShellNoCC {
            packages = [
              packages.SpaceWireRMAPLibrary

              pkgs.cmake
              pkgs.cmake-format
              pkgs.cmake-language-server
              pkgs.ninja

              clangTools
              pkgs.llvmPackages.lldb
              pkgs.llvmPackages.libcxxClang
            ];
            shellHook = ''
              export LLDB_DEBUGSERVER_PATH=/Library/Developer/CommandLineTools/Library/PrivateFrameworks/LLDB.framework/Versions/Current/Resources/debugserver
              export NIX_CFLAGS_COMPILE="$NIX_CFLAGS_COMPILE -B${pkgs.llvmPackages_19.libcxxClang.libcxx}/lib"
            '';
          };

          packages = rec {

            CxxUtilities = pkgs.stdenv.mkDerivation {
              pname = "cxx_utilities";
              version = "1.0.0"; # Replace with the appropriate version if needed.

              src = pkgs.fetchFromGitHub {
                owner = "yuasatakayuki";
                repo = "CxxUtilities";
                rev = "master"; # Use a specific commit or branch if necessary.
                sha256 = "sha256-C5pQJtpkHYTDxzpaZAFcDqozjBL1+i9Opx5vRp/l6uc="; # Replace with the actual hash.
              };

              installPhase = ''
                mkdir -p $out/include
                cp -r includes/* $out/include/
              '';
            };

            XMLUtilities = pkgs.stdenv.mkDerivation {
              pname = "XMLUtilities";
              version = "1.0.0";

              src = pkgs.fetchFromGitHub {
                owner = "sakuraisoki";
                repo = "XMLUtilities";
                rev = "master";
                sha256 = "sha256-7Bd65SWcH35hiZmC2XGNM/ytfttrbY7mWoFKRDgg/Lw="; # Replace with the actual hash.
              };

              installPhase = ''
                mkdir -p $out/include
                cp -r include/* $out/include/
              '';
            };

            SpaceWireRMAPLibrary = pkgs.stdenv.mkDerivation {
              pname = "SpaceWireRMAPLibrary";
              version = "1.0.0";

              src = pkgs.fetchFromGitHub {
                owner = "yuasatakayuki";
                repo = "SpaceWireRMAPLibrary";
                rev = "master";
                sha256 = "sha256-h0fw1qL/E+7VgoYSWlfS0sFwA1n+ZDZzYSStMwg8tAY=";
              };

              propagatedBuildInputs = [
                pkgs.xercesc
                CxxUtilities
                XMLUtilities
              ];

              installPhase = ''
                mkdir -p $out/include
                cp -r includes/* $out/include/
              '';

            };

            SpwRmap = pkgs.llvmPackages.libcxxStdenv.mkDerivation {
              pname = "spw-rmap";
              version = "1.0.0";
              src = ./.;

              nativeBuildInputs = [
                pkgs.cmake
                pkgs.ninja
                clangTools
                pkgs.llvmPackages.libcxxClang
              ];

              buildInputs = [
                SpaceWireRMAPLibrary
              ];
              configureFlags = [
                "-DSPWRMAP_BUILD_EXAMPLES=ON"
              ];
            };
          };
        };
    };
}
