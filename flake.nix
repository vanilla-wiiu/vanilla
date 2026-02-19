{
  description = "A software clone of the Wii U gamepad for Linux";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
  outputs = { self, nixpkgs }: {
    packages = nixpkgs.lib.genAttrs nixpkgs.lib.platforms.linux (system: {
      default = (import nixpkgs { inherit system; }).callPackage ./default.nix { };
    });
  };
}
