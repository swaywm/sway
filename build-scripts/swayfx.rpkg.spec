# vim: syntax=spec
Name:			{{{ git_dir_name }}}
Version:		0.1
Release:		1%{?dist}
Summary:		SwayFX: Sway, but with eye candy!
License:		MIT
URL:			https://github.com/WillPower3309/swayfx
VCS:			{{{ git_dir_vcs }}}
Source:			{{{ git_dir_pack }}}

BuildRequires:	gcc-c++
BuildRequires:	gnupg2
BuildRequires:	meson >= 0.60.0
BuildRequires:	pkgconfig(cairo)
BuildRequires:	pkgconfig(gdk-pixbuf-2.0)
BuildRequires:	pkgconfig(json-c) >= 0.13
BuildRequires:	pkgconfig(libdrm)
BuildRequires:	pkgconfig(libevdev)
BuildRequires:	pkgconfig(libinput) >= 1.6.0
BuildRequires:	pkgconfig(libpcre)
BuildRequires:	pkgconfig(libsystemd) >= 239
BuildRequires:	pkgconfig(libudev)
BuildRequires:	pkgconfig(pango)
BuildRequires:	pkgconfig(pangocairo)
BuildRequires:	pkgconfig(scdoc)
BuildRequires:	pkgconfig(wayland-client)
BuildRequires:	pkgconfig(wayland-cursor)
BuildRequires:	pkgconfig(wayland-egl)
BuildRequires:	pkgconfig(wayland-server) >= 1.20.0
BuildRequires:	pkgconfig(wayland-protocols) >= 1.24
BuildRequires:	(pkgconfig(wlroots) >= 0.15.0 with pkgconfig(wlroots) < 0.16)
BuildRequires:	pkgconfig(xcb)
BuildRequires:	pkgconfig(xkbcommon)
# Dmenu is the default launcher in sway
Recommends:		dmenu
# In addition, xargs is recommended for use in such a launcher arrangement
Recommends:		findutils
# Install configs and scripts for better integration with systemd user session
Recommends:		sway-systemd

Requires:		swaybg
Recommends:		swayidle
Recommends:		swaylock
# By default the Fedora background is used
Recommends:		desktop-backgrounds-compat

# Lack of graphical drivers may hurt the common use case
Recommends:		mesa-dri-drivers
# Minimal installation doesn't include Qt Wayland backend
Recommends:		(qt5-qtwayland if qt5-qtbase-gui)
Recommends:		(qt6-qtwayland if qt6-qtbase-gui)

# dmenu (as well as rxvt any many others) requires XWayland on Sway
Requires:		xorg-x11-server-Xwayland
# Sway binds the terminal shortcut to one specific terminal. In our case foot
Recommends:		foot
# grim is the recommended way to take screenshots on sway 1.0+
Recommends:		grim
%{?systemd_requires}

%description
SwayFX: Sway, but with eye candy!

%prep
{{{ git_dir_setup_macro }}}

%build
%meson \
	-Dsd-bus-provider=libsystemd \
	-Dwerror=false

%meson_build

%install
%meson_install

%files
%license LICENSE
%doc README.md
%{_bindir}/sway-git
%{_bindir}/swaybar-git
%{_datadir}/wayland-sessions/sway-git.desktop

# Changelog will be empty until you make first annotated Git tag.
%changelog
{{{ git_dir_changelog }}}
