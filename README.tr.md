# sway

[English][en] - [日本語][ja] - [Français][fr] - [Українська][uk] - [Español][es] - [Polski][pl] - [中文-简体][zh-CN] - [Deutsch][de] - [Nederlands][nl] - [Русский][ru] - [中文-繁體][zh-TW] - [Português][pt] - [Danish][dk] - [한국어][ko] - [Română][ro] - **[Türkçe][tr]**

Sway, [i3]-uyumlu bir [Wayland] dizgicisidir. [SSS][FAQ]'yi okuyun. 
[IRC kanalı][IRC channel]na katılın \(irc.freenode.net'te #sway (İngilizce)).

## Sürüm imzaları

Sürümler [E88F5E48] ile imzalandı ve [GitHub][GitHub releases]'da yayınlandı.

## Kurulum

### Paketler ile

Sway birçok dağıtımda mevcuttur. Sizinki için "sway" paketini yüklemeyi deneyin.

Dağıtımınız için sway'i paketlemekle ilgileniyorsanız, IRC kanalına uğrayın veya tavsiye için sir@cmpwn.com adresine bir e-posta gönderin.

### Kaynak koddan derleme

Test veya geliştirme için sway ve wlroots'un HEAD'ini oluşturmak istiyorsanız [bu wiki sayfası][Development setup]na göz atın.

Aşağıdaki bağımlılıkları yükleyin:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (isteğe bağlı: system tray)
* [scdoc] (isteğe bağlı: man pages) \*
* git (isteğe bağlı: version info) \*

_\*Derleme-anı bağımlılıkları_

Şu komutları çalıştırın:

    meson build
    ninja -C build
    sudo ninja -C build install

logind olmayan sistemlerde, sway ikilisine(binary) izin vermeniz (suid) gerekir:

    sudo chmod a+s /usr/local/bin/sway

Sway, başlangıçtan kısa bir süre sonra kök(root) izinlerini bırakacaktır.

## Yapılandırma

Zaten i3 kullanıyorsanız, i3 yapılandırmanızı `~/.config/sway/config` konumuna kopyalayın ve kutudan çıktığı gibi çalışacaktır. Aksi takdirde, örnek yapılandırma dosyasını `~/.config/sway/config` konumuna kopyalayın. Genellikle `/etc/sway/config` konumunda bulunur.
Yapılandırma hakkında bilgi almak için `man 5 sway` komutunu çalıştırın.

## Çalıştırma

TTY'den `sway` çalıştırın. Bazı  görüntü yöneticileriyle(display manager) çalışabilir ama Sway tarafından desteklenmez. (gdm'nin oldukça iyi çalıştığı bilinmektedir.)

[en]: https://github.com/swaywm/sway#readme
[ja]: https://github.com/swaywm/sway/blob/master/README.ja.md
[fr]: https://github.com/swaywm/sway/blob/master/README.fr.md
[uk]: https://github.com/swaywm/sway/blob/master/README.uk.md
[es]: https://github.com/swaywm/sway/blob/master/README.es.md
[pl]: https://github.com/swaywm/sway/blob/master/README.pl.md
[zh-CN]: https://github.com/swaywm/sway/blob/master/README.zh-CN.md
[de]: https://github.com/swaywm/sway/blob/master/README.de.md
[nl]: https://github.com/swaywm/sway/blob/master/README.nl.md
[ru]: https://github.com/swaywm/sway/blob/master/README.ru.md
[zh-TW]: https://github.com/swaywm/sway/blob/master/README.zh-TW.md
[pt]: https://github.com/swaywm/sway/blob/master/README.pt.md
[dk]: https://github.com/swaywm/sway/blob/master/README.dk.md
[ko]: https://github.com/swaywm/sway/blob/master/README.ko.md
[ro]: https://github.com/swaywm/sway/blob/master/README.ro.md
[tr]: https://github.com/swaywm/sway/blob/master/README.tr.md
[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC channel]: http://webchat.freenode.net/?channels=sway&uio=d4
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://github.com/swaywm/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
