# sway [![](https://api.travis-ci.org/SirCmpwn/sway.svg)](https://travis-ci.org/SirCmpwn/sway) [![Donate with fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

注・私の日本語で苦手です。パッチは与えったください。

"**S**irCmpwnの**Way**landのcompositor"は仕掛け品のWaylandのCompositorだ。
[英語のよくある質問](https://github.com/SirCmpwn/sway/wiki)は読みください。
[IRCのチャット](http://webchat.freenode.net/?channels=sway&uio=d4)(#swayにirc.freenode.net)は入ってください。

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

あなたが施したいから、[Patreon](https://patreon.com/sircmpwn)か[報奨金プロジェクト](https://github.com/SirCmpwn/sway/issues/986)は見てください。

## 日本語助け

SirCmpwnはIRCとGitHubでUTC-4に日本語助け人だ。

日本語翻訳は完成じゃない、正しくない。[パッチは与えったください](https://github.com/SirCmpwn/sway/issues/1318)。

## 発売の電子署名

発売は[B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)に調印して[GitHub](https://github.com/SirCmpwn/sway/releases)に発売する。

## 進展

- [i3のコンパチ](https://github.com/SirCmpwn/sway/issues/2)
- [IPCのコンパチ](https://github.com/SirCmpwn/sway/issues/98)
- [i3barのコンパチ](https://github.com/SirCmpwn/sway/issues/343)
- [i3-gapsのコンパチ](https://github.com/SirCmpwn/sway/issues/307)
- [無難の進むの](https://github.com/SirCmpwn/sway/issues/984)

## インスコ

### パッケージ

Swayは多いディストリビューションにある。
"sway"のパッケージは据え付ける。
パッケージはなかったば[wiki](https://github.com/SirCmpwn/sway/wiki/Unsupported-packages)が読みください。

### コンパイル

依存性インスコ:

* cmake
* [wlc](https://github.com/Cloudef/wlc)
* wayland
* xwayland
* libinput >= 1.6.0
* libcap
* asciidoc
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 *
* pam *
* imagemagick *
* ffmpeg *

\* 任意

実効する:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

logindはあるば:

    sudo setcap "cap_sys_ptrace,cap_sys_tty_config=eip" /usr/bin/sway

logindはあるないば:

    sudo chmod a+s /usr/local/bin/sway

## コンフィギュレーション

今はi3があるばi3のコンフィグは`~/.config/sway/config`にコピー。
あるないば`/etc/sway/config`をコピー。
ファイルは読みください。

## swayは実効する

ttyに"sway"は実効する。
若干ディスプレイマネージャもいいです、でも支援があるない。
