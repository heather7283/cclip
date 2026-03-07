{
  description = "clipboard manager for wayland";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      packages.${system} = {
        cclip = pkgs.stdenv.mkDerivation {
          pname = "cclip";
          version = "3.3.1";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            meson
            ninja
            pkg-config
            wayland-scanner
            git
          ];

          buildInputs = with pkgs; [ wayland sqlite xxHash ];
        };
        default = self.packages.${system}.cclip;
      };
    };
}
