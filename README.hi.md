# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - **[हिन्दी][hi]** - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

sway एक [i3]-अनुकूल
[Wayland] Compositor है।
[FAQ] पढिये। [IRC
Channel]
(irc.libera.chat पर #sway) में भी जुडिये।

## रिलीज हस्ताक्षर

रिलीजें
[E88F5E48]
से साइन होतें हैं और [Github पर][Github releases] प्रकाशित होते हैं।

## इंस्टौलेशन

### पैकेजों के द्वारा

Sway कई distributions में उप्लब्ध है। आप अपने में "sway" नामक पैकेज इंस्टौल करके देख
सकते हैं।

### Source से compile करके

यदि आप परीक्षण और विकास के लिए sway और wlroots के नवीनतम संस्करण बनाना
चाहते हैं, तो [यह विकी
पृष्ठ][Development-Setup] देखें।

निर्भरताएं:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf (वैकल्पिक: system tray के लिये)
* [scdoc] (वैकल्पिक: man पृष्ठों के लिये)
  \*
* git (वैकल्पिक: संस्करण जानने के लिये)

_\* Compilation के समय आवश्यक_

ये commands चलाएं:

	meson build/
	ninja -C build/
	sudo ninja -C build/ install

## Configuration

अगर आप पहले से ही i3 का उपयोग करते हैं तो अपने i3 config को
`~/.config/sway/config` में copy कर लीजिये और वह बिना किसी परिवर्तन के काम
करेगा। अन्यथा, नमूने configuration file को `~/.config/sway/config` में copy
कर लीजिये। यह सामान्यतः `/etc/sway/config` में पाया जाता है। `man 5
sway` से आप configuration के बारे में जानकारी प्राप्त कर सकते हैं।

## चलाना

आप एक tty से `sway` को चला सकते हैं। कुछ display managers काम करते हैं परन्तु ये
sway के द्वारा समर्थित नहीं है (gdm के बारे में जाना गया है कि वह सही काम करता
है)।

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
