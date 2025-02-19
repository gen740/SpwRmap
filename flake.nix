{
  description = "Flake shell";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  inputs.mynixpkgs.url = "git+ssh://git/gen740/mynixpkgs";

  outputs =
    {
      nixpkgs,
      mynixpkgs,
      ...
    }:
    let
      forSystems =
        attrs: systems:
        builtins.foldl' (
          result: system:
          nixpkgs.lib.attrsets.recursiveUpdate result (
            builtins.foldl' (
              res: prefix:
              nixpkgs.lib.attrsets.recursiveUpdate res {
                ${prefix}.${system} = builtins.getAttr prefix (attrs system);
              }
            ) { } (builtins.attrNames (attrs system))
          )
        ) { } systems;
      pkgs = nixpkgs.legacyPackages."x86_64-linux";
    in
    forSystems (
      system:
      with nixpkgs.legacyPackages.${system};
      with mynixpkgs.packages.${system};
      {
        devShells.default = mkShellNoCC {
          packages = [
            spacewire_rmap_library

            curl.dev
            cmake
            cmake-format
            cmake-language-server
            ninja

            (llvmPackages.clang-tools.override { enableLibcxx = true; })
            llvmPackages.libcxxClang
          ];
          shellHook = ''
            export NIX_CFLAGS_COMPILE="$NIX_CFLAGS_COMPILE -B${pkgs.llvmPackages_19.libcxxClang.libcxx}/lib";
          '';
        };
        packages.default = llvmPackages.libcxxStdenv.mkDerivation {
          name = "spw-rmap";
          buildInputs = [
            spacewire_rmap_library

            cmake
            cmake-format
            cmake-language-server
            ninja

            (llvmPackages.clang-tools.override { enableLibcxx = true; })
            llvmPackages.libcxxClang

          ];
          comfigureFlags = [
            "-DSPWRMAP_BUILD_EXAMPLES=ON"
          ];
          src = ./.;

        };
        # packages.default = pkgs.mkShell { packages = [ ]; };
      }
    ) nixpkgs.lib.platforms.all;
}
