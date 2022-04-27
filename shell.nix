let
  unstableTarball = fetchTarball https://github.com/NixOS/nixpkgs/archive/nixos-unstable.tar.gz;
  pkgs = import <nixpkgs> {}; 
  unstable = import unstableTarball {};

  shell = pkgs.mkShell {
    depsBuildBuild = with unstable; [
      pkg-config
    ];

    nativeBuildInputs = with unstable; [
      cmake meson ninja pkg-config wayland-scanner scdoc
    ];

    buildInputs = with unstable; [
      wayland libxkbcommon pcre json_c libevdev
      pango cairo libinput libcap pam gdk-pixbuf librsvg
      wayland-protocols libdrm wlroots dbus xwayland

      # wlroots
      libGL pixman xorg.xcbutilwm xorg.libX11 libcap xorg.xcbutilimage xorg.xcbutilerrors mesa
      libpng ffmpeg xorg.xcbutilrenderutil seatd
    ];
  };
in shell
