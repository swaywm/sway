# sway

[English][en] - [日本語][ja] - [Français][fr] - [Українська][uk] - [Español][es] - [Polski][pl] - [中文-简体][zh-CN] - [Deutsch][de] - [Nederlands][nl] - [Русский][ru] - [中文-繁體][zh-TW] - [Português][pt] - [Danish][dk] - [한국어][ko] - [Română][ro] - **[Türkçe][tr]**

sway [i3]-uyumlu bir [Wayland] dizgicisidir. [SSS]'i okuyun.
[IRC kanalı]'na katılın \(#sway on irc.freenode.net).

## Sürüm İmzalaarı

Sürümler [E88F5E48] ile imzalanmış ve [GitHub][GitHub sürümleri]'da yayınlanmıştır.

## Kurulum

### Paket İle Kurulum

Sway birçok dağıtımda mevcuttur. Dağıtımınızın paket yöneticisini kullanarak "sway" paketini yüklemeyi deneyin.

Sway'i dağıtımınız için paketlemekle ilgileniyorsanız, IRC kanalına uğrayın veya tavsiye için sir@cmpwn.com adresine bir e-posta gönderin.

### Kaynak Kod İle Kurulum

Sway ve wlroots'u test ve geliştirme amaçlı kurmak istiyorsanız bu [wiki sayfası][Development setup]'na göz atın.

Bağımlılıkları yükleyin:

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

_\*Derleme zamanı bağımlılıkları_

Bu komutları çalıştırın:

    meson build
    ninja -C build
    sudo ninja -C build install
    
Logind olmayan sistemlerde, sway binarylerine izin vermeniz gerekir:

    sudo chmod a+s /usr/local/bin/sway

Sway, başlangıçtan kısa bir süre sonra kök izinlerini bırakacaktır.

## Yapılandırma

Eğer hâlihazırda i3 kullanıyorsanız yapılandırma dosyanızı `~/.config/sway/config` konumuna kopyalamanız yeterli olacaktır. 
Eğer i3 kullanmıyorsanız `/etc/sway/config` konumundaki örnek yapılandırma dosyasını şuraya kopyalayın:
`~ /.config/sway/config`. 
Yapılandırma hakkında bilgi için `man 5 sway` komutunu çalıştırın.

## Çalıştırma

TTY ekranında `sway` komutunu çalıştırın. 
Bazı görüntü yöneticileri (display manager) çalışabilir ancak sway tarafından desteklenmemektedir (gdm'nin oldukça iyi çalıştığı bilinmektedir).

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
[SSS]: https://github.com/swaywm/sway/wiki
[IRC kanalı]: http://webchat.freenode.net/?channels=sway&uio=d4
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub sürümleri]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://github.com/swaywm/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
