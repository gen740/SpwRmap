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
          legacy_spw_rmap = (
            pkgs.stdenv.mkDerivation {
              pname = "space_wire_rmap_library";
              version = "1.0.0";

              src = pkgs.fetchFromGitHub {
                owner = "yuasatakayuki";
                repo = "SpaceWireRMAPLibrary";
                rev = "master";
                sha256 = "sha256-h0fw1qL/E+7VgoYSWlfS0sFwA1n+ZDZzYSStMwg8tAY=";
              };

              propagatedBuildInputs = [
                pkgs.xercesc
                (pkgs.stdenv.mkDerivation {
                  pname = "cxx_utilities";
                  version = "1.0.0";
                  src = pkgs.fetchFromGitHub {
                    owner = "yuasatakayuki";
                    repo = "CxxUtilities";
                    rev = "master";
                    sha256 = "sha256-C5pQJtpkHYTDxzpaZAFcDqozjBL1+i9Opx5vRp/l6uc=";
                  };

                  nativeBuildInputs = [ pkgs.perl ];

                  postPatch = ''
                    find . -name '*.hh' -print0 | while IFS= read -r -d "" file; do
                      echo "Processing $file"
                      perl -0777 -i -pe '
                        s{
                          \bthrow
                          \s*                     # optional whitespace
                          \(
                          (                      # capture group for inner content
                            (?:
                              [^()]*             # non-parens
                              (?:\([^()]*\)[^()]*)* # handle nested parens (1 level)
                            )
                          )
                          \)
                          (?!\s*;)               # negative lookahead: do not match if followed by ;
                        }{}gsx
                      ' "$file"
                    done
                  '';

                  installPhase = ''
                    mkdir -p $out/include
                    cp -r includes/* $out/include/
                  '';

                })
                (pkgs.stdenv.mkDerivation {
                  pname = "xml_utilities";
                  version = "1.0.0";
                  src = pkgs.fetchFromGitHub {
                    owner = "sakuraisoki";
                    repo = "XMLUtilities";
                    rev = "master";
                    sha256 = "sha256-7Bd65SWcH35hiZmC2XGNM/ytfttrbY7mWoFKRDgg/Lw=";
                  };
                  installPhase = ''
                    mkdir -p $out/include
                    cp -r include/* $out/include/
                  '';
                })
              ];

              nativeBuildInputs = [
                pkgs.perl
              ];

              postPatch = ''
                find . -name '*.hh' -print0 | while IFS= read -r -d "" file; do
                  echo "Processing $file"
                  perl -0777 -i -pe '
                    s{
                      \bthrow
                      \s*                     # optional whitespace
                      \(
                      (                      # capture group for inner content
                        (?:
                          [^()]*             # non-parens
                          (?:\([^()]*\)[^()]*)* # handle nested parens (1 level)
                        )
                      )
                      \)
                      (?!\s*;)               # negative lookahead: do not match if followed by ;
                    }{}gsx
                  ' "$file"
                done
              '';

              installPhase = ''
                mkdir -p $out/include
                cp -r includes/* $out/include/
              '';

            }
          );
        in
        {
          devShells.default = pkgs.mkShellNoCC {
            packages = [
              legacy_spw_rmap
              pkgs.cmake
              pkgs.cmake-format
              pkgs.cmake-language-server
              pkgs.ninja

              clangTools
              pkgs.llvmPackages.lldb
              pkgs.llvmPackages.libcxxClang
            ];
          };

          packages = {
            spw_rmap = pkgs.stdenv.mkDerivation {
              pname = "spw_rmap";
              version = "1.0.0";
              src = ./.;
              nativeBuildInputs = [
                pkgs.cmake
                pkgs.ninja
              ];
              buildInputs = [
                legacy_spw_rmap
              ];
            };
          };

          apps = {
            build = {
              type = "app";
              program =
                (pkgs.writeShellScript "build-spw-rmap" ''
                  if [ ! -d build ]; then
                    nix develop --command cmake -S . -B build -G Ninja \
                      -DCMAKE_BUILD_TYPE=Debug \
                      -DSPWRMAP_BUILD_EXAMPLES=ON \
                      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
                  fi
                  nix develop --command cmake --build build
                '').outPath;
            };
            check = {
              type = "app";
              program =
                (pkgs.writeShellScript "build-spw-rmap" ''
                  echo "Running clang-tidy ..."
                  ${pkgs.ripgrep}/bin/rg -0 -tcpp -l . | \
                    xargs -0 -n 1 clang-tidy -p ./build
                  echo "Running clang-format ..."
                  ${pkgs.ripgrep}/bin/rg -0 -tcpp -l . | \
                    xargs -0 -n 1 -P 8 clang-format -i
                  echo "OK"
                '').outPath;
            };

          };
        };
    };
}
