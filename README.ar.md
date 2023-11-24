# sway

sway 

هو مدير للمجموعات المركبة لـ[Wayland] متوافق مع [i3] -

إقرأ [الأسئلة الشائعة](https://github.com/swaywm/sway/wiki)

 انضم الى [قناة IRC](https://web.libera.chat/gamja/?channels=#sway)


## تواقيع الإصدار
 تٌوقع الإصدارات بـواسطة [E88F5E48] و تُنشر على [GitHub](https://github.com/swaywm/sway/releases)

## التثبيت

### بإستخدام الحزم

يتوفر Sway للعديد من التوزيعات، حاول تثبيت حزمة "sway" لتوزيعتك
### التجميع من المصدر
إطلع على [صفحة الويكي هذه](https://github.com/swaywm/sway/wiki/Development-Setup) إذا أردت بناء الـHEAD من sway و wlroots لأغراض الفحص والتطوير 

تثبيت اللوازم: 
* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (optional: system tray)
* [scdoc] (optional: man pages) \*
* git (optional: version info) \*

_\* Compile-time dep_

نفذ هذه الأوامر:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

## الإعدادات

إذا كنت بالفعل تستخدم i3، فعليك نسخ إعدادات i3 لديك إلى  `~/.config/sway/config` وسوف تعمل تلقائياً.

و إلا عليك نسخ ملف الإعدادات النموذج إلى `config/sway/config` الموجود عادةً في `/etc/sway/config.` 


## التشغيل

شغل `sway` بإستخدام أمر TTY. 
قد يعمل بعض مدراء العرض مع أنهم غير مدعومون من sway 
(gdm مثلاً يعمل بشكل جيد إلى حد ما)

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
[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC channel]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
