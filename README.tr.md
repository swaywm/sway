# sway

sway, [i3](https://i3wm.org/)-uyumlu bir [Wayland](http://wayland.freedesktop.org/) birleştiricisidir.
[S.S.S.](https://github.com/swaywm/sway/wiki)'yi okuyun. [IRC
kanalına](http://webchat.freenode.net/?channels=sway&uio=d4) katılın (irc.freenode.net
üzerinde #sway).

sway'in geliştirilmesini desteklemek istiyorsanız, lütfen [SirCmpwn'in Patreon
sayfasına](https://patreon.com/sircmpwn) katkıda bulunun.

## Sürüm İmzaları

Sürümler [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A) anahtarıyla imzalanır
ve [GitHub'da](https://github.com/swaywm/sway/releases) yayınlanır.

## Kurulum

### Paketlerden

sway, birçok dağıtımın depolarında bulunabilir. Kullandığınız dağıtımda "sway" paketini kurmayı
deneyin. Eğer yoksa, dağıtımınızla ilgili kurulum bilgileri için [bu wiki
sayfasına](https://github.com/swaywm/sway/wiki/Unsupported-packages) göz atın.

Dağıtımınız için sway'i paketlemekle ilgileniyorsanız, tavsiye için IRC kanalına
uğrayın veya sir@cmpwn.com adresine bir e-posta gönderin.

### Kaynaktan derleme

Test etmek veya geliştirmek için sway ve wlroots'un HEAD'ini derlemek istiyorsanız [bu wiki sayfasına](https://github.com/swaywm/sway/wiki/Development-Setup) göz atın.

Kurulum bağımlılıkları:

* meson \*
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (isteğe bağlı: sistem bildirim alanı)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (isteğe bağlı: kılavuz sayfaları) \*
* git (isteğe bağlı: sürüm bilgileri) \*

_\*Derleme zamanı bağımlılığı_

Aşağıdaki komutları çalıştırın:

    meson build
    ninja -C build
    sudo ninja -C build install

logind olmayan sistemlerde, sway ikili dosyasını suid olarak ayarlamanız gerekir:

    sudo chmod a+s /usr/local/bin/sway

sway, başlangıçtan kısa bir süre sonra root izinlerini bırakacaktır.

## Yapılandırma

Zaten i3 kullanıyorsanız, i3 yapılandırma dosyanızı `~/.config/sway/config`
konumuna kopyalayın, sorunsuz çalışacaktır. Aksi takdirde örnek yapılandırma dosyasını
`~/.config/sway/config` konumuna kopyalayın. Genelde `/etc/sway/config` konumunda
bulunur. Yapılandırma hakkında bilgiler için `man 5 sway` komutunu çalıştırın.

## Çalıştırma

TTY üzerinden `sway` komutunu çalıştırın. Bazı görüntü yöneticileri çalışabilir ancak
sway tarafından desteklenmez (bilindiği kadarıyla gdm oldukça iyi çalışıyor).
