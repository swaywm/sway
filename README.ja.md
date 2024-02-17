# sway

[English][en] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - **[日本語][ja]** - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Svenska][sv] - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

Swayは[i3]互換な[Wayland]コンポジタです。
[FAQ]も合わせてご覧ください。
[IRC チャンネル][IRC channel] (irc.libera.chatの#sway)もあります。

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

## 日本語サポート

SirCmpwnは、日本語でのサポートをIRCとGitHubで行います。タイムゾーンはUTC-4です。

## リリースの署名

Swayのリリースは[E88F5E48]で署名され、[GitHub][Github releases]で公開されています。

## インストール

### パッケージから

Swayは沢山のディストリビューションで提供されています。"sway"パッケージのインストールを試してください。パッケージが存在しない場合は、[このページ](https://github.com/swaywm/sway/wiki/Unsupported-packages)で、あなたのディストリビューションでのインストールに関する情報を調べてください。

あなたのディストリビューションにSwayのパッケージを提供したい場合は、SwayのIRCチャンネルを訪れるか、sir@cmpwn.comにメールを送り、相談してください。

### ソースコードからコンパイル

次の依存パッケージをインストールしてください:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (任意: システムイコンで必要です)
* [scdoc] (任意: manで必要です) \*
* git (任意: バージョン情報で必要です) \*

_\*コンパイル時の依存_

次のコマンドを実行してください:

    meson build
    ninja -C build
    sudo ninja -C build install

## 設定

既にi3を使用している場合は、i3の設定ファイルを`~/.config/sway/config`にコピーすれば動きます。そうでない場合は、サンプルの設定ファイルを`~/.config/sway/config`にコピーしてください。サンプルの設定ファイルは、通常`/etc/sway/config`にあります。`man 5 sway`を実行することで、設定に関する情報を見ることができます。

## 実行

`sway`をTTYから実行してください。いくつかのディスプレイマネージャは動くかもしれませんが、Swayからサポートされていません(gdmは非常に良く動作することが知られています)。

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
