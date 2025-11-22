{
  description = "Flake shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs =
    inputs@{
      flake-parts,
      ...
    }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [
        "aarch64-darwin"
        "x86_64-darwin"
        "aarch64-linux"
        "x86_64-linux"
      ];

      flake.overlays.default = final: prev: {
        pythonPackagesExtensions = (prev.pythonPackagesExtensions or [ ]) ++ [
          (python-final: python-prev: {
            pyspw_rmap = python-final.buildPythonPackage {
              pname = "pyspw_rmap";
              version = "0.0.1";
              format = "pyproject";
              src = ./.;
              nativeBuildInputs =
                (with python-final; [
                  scikit-build-core
                  pybind11
                  pybind11-stubgen
                  wheel
                ])
                ++ (with final; [
                  cmake
                  ninja
                ]);
              pythonImportsCheck = [ "pyspw_rmap" ];
              dontUseCmakeConfigure = true;
              dontUseCmakeBuild = true;
              dontUseCmakeInstall = true;
            };
          })
        ];
      };

      perSystem =
        { pkgs, ... }:
        {
          devShells.default = pkgs.mkShell {
            packages = [
              pkgs.cmake
              pkgs.cmake-format
              pkgs.cmake-language-server
              pkgs.ninja
              pkgs.gtest.dev
              pkgs.ruff
              pkgs.nixfmt
              (pkgs.python313.withPackages (
                ps: with ps; [
                  pybind11
                  pybind11-stubgen
                ]
              ))
              pkgs.python313Packages.venvShellHook
            ];
            venvDir = "venv";
          };

          packages = {
            spw_rmap = pkgs.stdenv.mkDerivation {
              pname = "spw_rmap";
              version = "1.0.0";
              src = ./.;
              nativeBuildInputs = [
                pkgs.cmake
                pkgs.ninja
                (pkgs.python313.withPackages (
                  ps: with ps; [
                    pybind11
                    pybind11-stubgen
                  ]
                ))
              ];
              cmakeFlags = [
                "-DSPWRMAP_BUILD_PYTHON_BINDINGS=ON"
                "-DSPWRMAP_BUILD_TESTS=ON"
              ];
              buildInputs = [
                pkgs.gtest.dev
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
                      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
                      -DCMAKE_BUILD_TYPE=Debug \
                      -DSPWRMAP_BUILD_TESTS=ON \
                      -DSPWRMAP_BUILD_EXAMPLES=ON \
                      -DSPWRMAP_BUILD_PYTHON_BINDINGS=ON
                  fi
                  nix develop --command cmake --build build
                '').outPath;
            };
          };

          formatter = pkgs.treefmt;
        };
    };
}
