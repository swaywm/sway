# vim: syntax=spec

### CHANGE THESE VARIABLES BEFORE RELEASE:
# Change to current Sway base version!
%global SwayBaseVersion 1.8.1
# Change to current SwayFX tag!
%global Tag 0.1.1

Name:			{{{ git_dir_name }}}
Version:		%{Tag}
Release:		2%{?dist}
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
BuildRequires:	pkgconfig(libinput) >= 1.21.0
BuildRequires:	pkgconfig(libpcre2)
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
BuildRequires:	(pkgconfig(wlroots) >= 0.16.0 with pkgconfig(wlroots) < 0.17)
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

Conflicts:		sway
Provides:		sway = %{SwayBaseVersion}

%description
SwayFX: Sway, but with eye candy!

# The artwork is heavy and we don't use it with our default config
%package        wallpapers
Summary:        Wallpapers for Sway
BuildArch:      noarch
License:        CC0

%description    wallpapers
Wallpaper collection provided with Sway


%package -n     grimshot
Summary:        Helper for screenshots within sway
Requires:       grim
Requires:       jq
Requires:       slurp
Requires:       /usr/bin/wl-copy
Recommends:     /usr/bin/notify-send

%description -n grimshot
Grimshot is an easy to use screenshot tool for sway. It relies on grim,
slurp and jq to do the heavy lifting, and mostly provides an easy to use
interface.

%prep
{{{ git_dir_setup_macro }}}

%build
%meson \
    -Dsd-bus-provider=libsystemd \
    -Dwerror=false

%meson_build

%install
%meson_install
# Set Fedora background as default background
sed -i "s|^output \* bg .*|output * bg /usr/share/backgrounds/default.png fill|" %{buildroot}%{_sysconfdir}/sway/config
# Create directory for extra config snippets
install -d -m755 -pv %{buildroot}%{_sysconfdir}/sway/config.d

# install python scripts from contrib
install -D -m644 -pv -t %{buildroot}%{_datadir}/sway/contrib contrib/*.py

# install contrib/grimshot tool
scdoc <contrib/grimshot.1.scd >%{buildroot}%{_mandir}/man1/grimshot.1
install -D -m755 -pv contrib/grimshot %{buildroot}%{_bindir}/grimshot

%files
%license LICENSE
%doc README.md
%dir %{_sysconfdir}/sway
%dir %{_sysconfdir}/sway/config.d
%config(noreplace) %{_sysconfdir}/sway/config
%{_mandir}/man1/sway*
%{_mandir}/man5/*
%{_mandir}/man7/*
%{_bindir}/sway
%{_bindir}/swaybar
%{_bindir}/swaymsg
%{_bindir}/swaynag
%{_datadir}/sway
%{_datadir}/wayland-sessions/sway.desktop
%dir %{_datadir}/zsh
%dir %{_datadir}/zsh/site-functions
%{_datadir}/zsh/site-functions/_sway*
%dir %{_datadir}/bash-completion
%dir %{_datadir}/bash-completion/completions
%{_datadir}/bash-completion/completions/sway*
%dir %{_datadir}/fish
%dir %{_datadir}/fish/vendor_completions.d
%{_datadir}/fish/vendor_completions.d/sway*

%files wallpapers
%license assets/LICENSE
%{_datadir}/backgrounds/sway

%files -n grimshot
%{_bindir}/grimshot
%{_mandir}/man1/grimshot.1*

# Changelog will be empty until you make first annotated Git tag.
%changelog
{{{ git_dir_changelog }}}
