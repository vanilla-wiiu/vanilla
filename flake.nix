{
  description = "A software clone of the Wii U gamepad for Linux";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    dhclient_nixpkgs.url = "github:NixOS/nixpkgs/8d34cf8e7dfd8f00357afffe3597bc3b209edf8e";
  };
  outputs = { self, nixpkgs, dhclient_nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
      pkgsFor = nixpkgs.legacyPackages;
    in
    rec {
      # Complicated garbage to get dhclient from an old version of nixpkgs.
      dhcpkgs = forAllSystems
        (system: {
          pkgs = import dhclient_nixpkgs {
            config.permittedInsecurePackages = [
              "dhcp-4.4.3-P1" # yes, I love old EOL software
            ];
            system = system;
          };
        });
      packages = forAllSystems
        (system: {
          # Build for the host system
          default = pkgsFor.${system}.qt6Packages.callPackage ./default.nix {
            dhclient = dhcpkgs.${system}.pkgs.dhcp.override { withClient = true; };
          };
        });
    };
}

