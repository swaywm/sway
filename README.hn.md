# sway

sway एक [i3](https://i3wm.org/)-अनुकूल
[Wayland](https://wayland.freedesktop.org/) Compositor है।
[FAQ](https://github.com/swaywm/sway/wiki) पढिये। [IRC
Channel](https://web.libera.chat/gamja/?channels=#sway)
(irc.libera.chat पर #sway) में भी जुडिये।

## रिलीज सिग्नेचर

रिलीजें
[E88F5E48](https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48)
से साइन होतें हैं और [Github पर](https://github.com/swaywm/sway/releases) प्रकाशित होते हैं।

## इंस्टौलेशन

### पैकेजों के ज़रिये

Sway कई distributions में उप्लब्ध है। आप अपने में "sway" नामक पैकेज इंस्टौल करके देख
सकते हैं।

### Source से compile करके

यदि आप परीक्षण और विकास के लिए sway और wlroots का नवीनतम संस्करण बनाना
चाहते हैं, तो [यह विकी
पृष्ठ](https://github.com/swaywm/sway/wiki/Development-Setup) देखें।

निर्भरताएं:

* meson \*
* [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf (इच्छा पर: system tray के लिये)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (इच्छा पर: man पृष्ठों के लिये)
  \*
* git (इच्छा पर: संस्करण जानकारी के लिये)

_\* Compilation के समय आवश्यक_

ये commands चलाएं:

	meson build/
	ninja -C build/
	sudo ninja -C build/ install

उन systems पर जिनमें ना तो logind है, ना ही seatd है, आपको sway के binary
को suid करना पडेगा:

	sudo chmod a+s /usr/local/bin/sway

Sway अपने root अनुमतियां शुरू होने के कुछ ही देर बाद गिरा देगा।

## Configuration

अगर आप पहले से ही i3 का उप्योग करते हैं तो अपने i3 config को
`~/.config/sway/config` में copy कर लीजिये और वह बिना किसी अन्य मेहनत के काम
करेगा। अन्यथा, नमूने configuration file को `~/.config/sway/config` में copy
कर लीजिये। यह अधिकतर समय `~/etc/sway/config` में पाया जाता है। `man 5
sway` से आप configuration के बारे में जानकारी प्राप्त कर सकते हैं।

## चलाना

आप एक tty से `sway` को चला सकते हैं। कुछ display managers काम करते हैं मगर ये
sway के द्वारा समर्थित नहीं है (gdm के बारे में जाना गया है कि वह सही काम करता
है)।
