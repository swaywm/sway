{
  description = "swaywm development environment";

  inputs = {
    nixpkgs.url = "nixpkgs/nixpkgs-unstable";
    flake-utils = { url = "github:numtide/flake-utils"; };
  };

  outputs = {self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

      in {
        devShell = pkgs.mkShell {
          depsBuildBuild = with pkgs; [
            pkg-config
          ];
 
          nativeBuildInputs = with pkgs; [
            cmake meson ninja pkg-config wayland-scanner scdoc
          ];

          buildInputs = with pkgs; [
            wayland libxkbcommon pcre json_c libevdev pango cairo libinput libcap pam gdk-pixbuf librsvg
            wayland-protocols libdrm wlroots dbus xwayland
            # wlroots
            libGL pixman xorg.xcbutilwm xorg.libX11 libcap xorg.xcbutilimage xorg.xcbutilerrors mesa
            libpng ffmpeg xorg.xcbutilrenderutil seatd
          ];
        };
      }
    );
}
