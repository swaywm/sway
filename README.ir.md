# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - **[فارسی][ir]** - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

&rlm;sway یک کامپوزیتور الهام گرفته از [i3] بر روی [Wayland] است. [سوال‌های متداول][FAQ] را بخوانید. در [کانال
IRC][IRC channel] عضو شوید (&lrm;#sway&rlm; در
irc.libera.chat).

برای حمایت از تیم توسعه sway به [صفحه
Patreon با نام کاربری SirCmpwn](https://patreon.com/sircmpwn) مراجعه کنید.

## امضای نسخه‌ها

امضای نسخه‌ها با [E88F5E48] در [GitHub][Github releases] منتشر می‌شود.

## شیوه نصب

### از بسته‌های رسمی

&rlm;sway در بسته‌های رسمی توزیع‌های مختلف وجود دارد. بسته «sway» را نصب کنید. در صورتی که بسته رسمی وجود نداشت، برای آگاهی بیشتر درباره نصب روی توزیعتان به این [صفحه راهنما](https://github.com/swaywm/sway/wiki/Unsupported-packages) مراجعه کنید.

اگر به ایجاد بسته sway برای توزیعتان علاقه‌مند هستید، از کانال IRC استفاده کنید یا به sir@cmpwn.com ایمیل بزنید.

### کامپایل کردن کد

چنانچه می‌خواهید آخرین نسخه کد sway و wlroots را برای آزمایش یا توسعه بسازید به این [صفحه راهنما][Development setup] مراجعه کنید.

بسته‌های مورد نیاز:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (انتخابی: برای system tray)
* [scdoc] (انتخابی: برای صفحه‌های راهنما) \*
* git (انتخابی: برای اطلاع در خصوص نسخه‌ها) \*

_\*نیازمندی‌های زمان کامپایل برنامه_

این فرمان‌ها را اجرا کنید:

    meson build
    ninja -C build
    sudo ninja -C build install

روی سیستم‌های بدون logind، باید فرمان زیر را برای suid کردن باینری sway اجرا کنید:

    sudo chmod a+s /usr/local/bin/sway

&rlm;sway پس از startup مجوزهای دسترسی root را رها می‌کند.

### شخصی سازی و تنظیمات

اگر در حال حاضر از i3 استفاده می‌کنید، تنظیمات i3 خودتان را در فایل ‪`~/.config/sway/config`‬ کپی کنید و بدون نیاز به تغییر کار خواهد کرد. در غیر این‌صورت، فایل نمونه تنظیمات را استفاده کنید. این فایل عموما در ‪`/etc/sway/config`‬ قرار دارد. برای آگاهی بیشتر `man 5 sway` را اجرا کنید.

## اجرا

در محیط TTY کافیست `sway` را اجرا کنید. ممکن است ابزارهای مدیریت نمایشگری نیز برای این کار وجود داشته باشند اما از طرف sway پشتیبانی نمی‌شوند (gdm عملکرد خوبی در این زمینه دارد).

[en]: https://github.com/swaywm/sway#readme
[cs]: README.cs.md
[de]: README.de.md
[dk]: README.dk.md
[es]: README.es.md
[fr]: README.fr.md
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
