# Maintainer: Erik Reider <erik.reider@protonmail.com>
_pkgname=swayfx
pkgname="$_pkgname"
pkgver=0.1
pkgrel=1
license=("MIT")
pkgdesc="SwayFX: Sway, but with eye candy!"
makedepends=(
	"git"
	"meson"
	"scdoc"
	"wayland-protocols"
)
depends=(
	"cairo"
	"gdk-pixbuf2"
	"libevdev.so"
	"libinput"
	"libjson-c.so"
	"libudev.so"
	"libwayland-server.so"
	"libwlroots.so"
	"libxcb"
	"libxkbcommon.so"
	"pango"
	"pcre"
	"ttf-font"
	"wlroots<0.16"
)
optdepends=(
	"alacritty: Terminal emulator used by the default config"
	"dmenu: Application launcher"
	"grim: Screenshot utility"
	"i3status: Status line"
	"mako: Lightweight notification daemon"
	"slurp: Select a region"
	"swayidle: Idle management daemon"
	"swaylock: Screen locker"
	"wallutils: Timed wallpapers"
	"waybar: Highly customizable bar"
)
backup=(etc/sway/config)
arch=("i686" "x86_64")
url="https://github.com/WillPower3309/swayfx"
source=("${url}/releases/download/$pkgver/swayfx-$pkgver.tar.gz"
	50-systemd-user.conf)
sha512sums=(
	"SKIP"
	"c2b7d808f4231f318e03789015624fd4cf32b81434b15406570b4e144c0defc54e216d881447e6fd9fc18d7da608cccb61c32e0e1fab2f1fe2750acf812d3137")
provides=("sway" "swayfx")
conflicts=("sway" "swayfx")
options=(debug)
install=sway.install

build() {
	arch-meson \
		-Dsd-bus-provider=libsystemd \
		-Dwerror=false \
		"$_pkgname" build
	meson compile -C build
}

package() {
	install -Dm644 50-systemd-user.conf -t "$pkgdir/etc/sway/config.d/"

	DESTDIR="$pkgdir" meson install -C build

	cd "$_pkgname"
	install -Dm644 "LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
	for util in autoname-workspaces.py inactive-windows-transparency.py grimshot; do
		install -Dm755 "contrib/$util" -t "$pkgdir/usr/share/$pkgname/scripts"
	done
}
