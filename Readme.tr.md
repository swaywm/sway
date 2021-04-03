# sway

[English][en] - [日本語][ja] - [Français][fr] - [Українська][uk] - [Español][es] - [Polski][pl] - [中文-简体][zh-CN] - [Deutsch][de] - [Nederlands][nl] - [Русский][ru] - [中文-繁體][zh-TW] - [Português][pt] - **[Türkçe][tr]** - [Danish][dk] - [한국어][ko] - [Română][ro] - [Magyar][hu]

sway, i3 ile bağdaşlaşan ve wayland üzerinde çalışan bir pencere yöneticisidir. 
[SSS] okuyun. [IRC] kanalına katılın \(irc.freenode.net üzerinde #sway).

## Sürüm imzaları

Sürümler [E88F5E48] imzası ile imzalanır ve [GitHub][GitHub releases] üzerinden yayınlanır.

## Kurulum

### Paketler

Sway bazı distrolardan edinilebilir. Kullandığınız distro üzerinden sway paketini kurmayı deneyin.

Eğer kullandığınız dağıtım için sway paketleme ile uğraşıyorsanız IRC kanalına uğrayın.
Tavsiye için sir@cmpwn.com adresine bir e-posta ateşleyin.


### Kaynak kodlarını kullanarak derleme

Test veya geliştirme için sway ve wlroots HEAD'ini oluşturmak istiyorsanız.
[Bu wiki sayfasını][Development setup] kontrol edin. 

Kurulum bağımlılıkları:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (opsiyonel: sistem çekmecesi)
* [scdoc] (opsiyonel: man sayfaları) \*
* git (opsiyonel: versiyon bilgisi) \*

_\*ve Derleme süresi_

Bağımlılıkları edindikten sonra bu komutları çalıştırın:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

Logind kullanmayan bir sistem çalıştırıyorsanız, sway binary dosyasına gerekli izinleri tanımlamalısınız:

    sudo chmod a+s /usr/local/bin/sway

Sway başlangıçta root izni istemeyecektir.

## Konfigurasyon

Eğer halihazırda i3 kullanıyorsanız, i3 konfigurasyon dosyasını `~/.config/sway/config`
yoluna kopyalayın. Ardından çalıştığını kontrol edin. 
Konfigurasyon dosyanız yoksa örnek konfigurasyon dosyasını `~/.config/sway/config` 
yoluna kopyalayın. Genellikle örnek konfigurasyon dosyası `/etc/sway/config` yolundadır.
`man 5 sway` komutunu çalıştırarak konfigurasyon hakkında detaylı bilgi alabilirsiniz.


## Çalıştırma

TTY ekranında `sway` komutunu çalıştırın. Bazı görüntü yöneticileri tarafından çalıştırılabilir.
Ancak sway tarafından desteklenmez. (gdm ile iyi çalıştığı bilinir)

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
[tr]: https://github.com/swaywm/sway/blob/master/README.tr.md
[dk]: https://github.com/swaywm/sway/blob/master/README.dk.md
[ko]: https://github.com/swaywm/sway/blob/master/README.ko.md
[ro]: https://github.com/swaywm/sway/blob/master/README.ro.md
[hu]: https://github.com/swaywm/sway/blob/master/README.hu.md
[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[SSS]: https://github.com/swaywm/sway/wiki
[IRC]: http://webchat.freenode.net/?channels=sway&uio=d4
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://github.com/swaywm/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
