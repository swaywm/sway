# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - **[中文-繁體][zh-TW]**

sway 是一個與 [i3] 相容的 [Wayland] compositor。
閱讀 [FAQ]。 加入 [IRC
頻道][IRC channel] (#sway on
irc.libera.chat)

## 發行簽章

所有發行的版本都會以 [E88F5E48] 簽署
並發佈於 [GitHub][Github releases]

## 安裝

### 從套件安裝

Sway 在許多發行版都有提供。請自己嘗試於你的發行版安裝 「sway」這個套件。
如果無法取得，請查看 [這個 wiki 頁面](https://github.com/swaywm/sway/wiki/Unsupported-packages)
以取得更多關於如何於你使用的發行版上安裝的資訊。

如果你想要為你使用的發行版包裝 sway，請到 IRC 頻道或是直接寄封信到 sir@cmpwn.com 來取得一些建議。

### 從原始碼編譯

相依套件:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (選擇性: system tray)
* [scdoc] (選擇性: man pages) \*
* git \*

_\*編譯時相依_

執行這些指令:

    meson build
    ninja -C build
    sudo ninja -C build install

在沒有 logind 的系統上，你需要為 sway 的執行檔加上 suid。

    sudo chmod a+s /usr/local/bin/sway

Sway 在啟動不久後就會放棄 root 權限。

## 設定檔

如果你已經在使用 i3，你可以直接將你的 i3 設定檔複製到 `~/.config/sway/config` 然後就能直接使用。
或者你也可以把範例設定檔複製到 `~/.config/sway/config`。 它通常會在 `/etc/sway/config`。
執行 `man 5 sway` 來取得更多關於設定檔的資訊。

## 執行

在 TTY 執行 `sway`。有些 display manager 可能可以運作但 sway 不提供支援 (已知 gdm 運作的很好)

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
