# sway

sway هو مُركِّب (compositor) لـ [Wayland] متوافق مع [i3]. اطّلع على
[الأسئلة الشائعة][FAQ]. انضم إلى [قناة IRC][IRC channel] \(القناة #sway على irc.libera.chat).

## توقيعات الإصدارات

تُوقَّع الإصدارات بالمفتاح [E88F5E48] وتُنشر [على GitHub][GitHub releases].

## التثبيت

### من الحزم

يتوفر Sway في كثير من التوزيعات. جرّب تثبيت حزمة "sway" في توزيعتك.

### البناء من المصدر

اطّلع على [صفحة الويكي هذه][Development setup] إذا أردت بناء النسخة الأحدث (HEAD)
من sway وwlroots لأغراض الاختبار أو التطوير.

ثبّت المتطلبات:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (اختياري: صيغ صور إضافية لشريط النظام)
* [swaybg] (اختياري: خلفية الشاشة)
* [scdoc] (اختياري: صفحات الدليل man) \*
* git (اختياري: معلومات الإصدار) \*

_\* متطلب وقت البناء فقط_

نفّذ هذه الأوامر:

    meson setup build/
    ninja -C build/
    sudo ninja -C build/ install

## الإعدادات

إذا كنت تستخدم i3 مسبقاً، فانسخ ملف إعدادات i3 إلى `~/.config/sway/config`
وسيعمل مباشرة بدون أي تعديل. وإلا فانسخ ملف الإعدادات النموذجي إلى
`~/.config/sway/config`، وهو موجود عادةً في `/etc/sway/config`.
شغّل الأمر `man 5 sway` لمعرفة تفاصيل الإعدادات.

## التشغيل

شغّل `sway` من TTY أو من مدير العرض.

[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC channel]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[swaybg]: https://github.com/swaywm/swaybg/
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
[en]: https://github.com/swaywm/sway#readme
[ar]: README.ar.md
[cs]: README.cs.md
[de]: README.de.md
[dk]: README.dk.md
[es]: README.es.md
[fr]: README.fr.md
[ge]: README.ge.md
[gr]: README.gr.md
[hi]: README.hi.md
[hu]: README.hu.md
[ir]: README.ir.md
[it]: README.it.md
[ja]: README.ja.md
[ko]: README.ko.md
[nl]: README.nl.md
[no]: README.no.md
[pl]: README.pl.md
[pt]: README.pt.md
[ro]: README.ro.md
[ru]: README.ru.md
[sv]: README.sv.md
[tr]: README.tr.md
[uk]: README.uk.md
[zh-CN]: README.zh-CN.md
[zh-TW]: README.zh-TW.md
