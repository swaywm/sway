# sway [![](https://api.travis-ci.org/swaywm/sway.svg)](https://travis-ci.org/swaywm/sway) [![Donate with fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

"**S**irCmpwn's **Way**land compositor"は**開発中**の
i3互換な[Wayland](http://wayland.freedesktop.org/)コンポジタです。
[FAQ](https://github.com/swaywm/sway/wiki)も合わせてご覧ください。
[IRC チャンネル](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway on
irc.freenode.net)もあります。

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

もしSwayの開発を支援したい場合は[SirCmpwnのPatreon](https://patreon.com/sircmpwn)や
[こちら](https://github.com/swaywm/sway/issues/986)をご覧ください。

誰でも賞金を受け取る事ができますし、自分の欲しい機能に賞金を掛ける事が出来ます。
PatreonはSwayの開発を支援するのにもっとも便利です。

## リリース

Swayのリリースは[B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)で書名されて
[GitHub](https://github.com/swaywm/sway/releases)で公開されています。

## 開発状況

- [i3のサポート](https://github.com/swaywm/sway/issues/2)
- [IPCのサポート](https://github.com/swaywm/sway/issues/98)
- [i3barのサポート](https://github.com/swaywm/sway/issues/343)
- [i3-gapsのサポート](https://github.com/swaywm/sway/issues/307)
- [セキュリティ対応](https://github.com/swaywm/sway/issues/984)

## インストール

### パッケージから

Swayは沢山のディストリビューションで提供されています。"sway"パッケージをインストールしてみてください。
もし、パッケージが存在しないならば、[このページ](https://github.com/swaywm/sway/wiki/Unsupported-packages)
を参照してインストールしてみてください。

もし、Swayのパッケージを提供したいならば、SwayのIRCチャンネルか"sir@cmpwn.com"に連絡してください。

### ソースコードから

まずはコンパイルや実行に必要なソフトウェアやライブラリをインストールしてください。:

* cmake
* [wlc](https://github.com/Cloudef/wlc)
* wayland
* xwayland
* libinput >= 1.6.0
* libcap
* asciidoc
* pcre
* json-c <= 0.12.1
* pango
* cairo
* gdk-pixbuf2 *
* pam **
* imagemagick (swaygrabでスクリーンショットを撮るのに必要です)
* ffmpeg (swaygrabで画面を録画するのに必要です)

_\*swaybar,swaybg,swaylockが使用します_

_\*\*swaylockが使用します_

ターミナルで次のコマンドを実行してください:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
    make
    sudo make install

logindを使用している場合はバイナリにcapを設定してください:

    sudo setcap "cap_sys_ptrace,cap_sys_tty_config=eip" /usr/bin/sway

logindを使用していない場合はバイナリにsuidを設定してください:

    sudo chmod a+s /usr/local/bin/sway

## 設定

もし、既にi3を使用しているなら、i3のコンフィグファイルを`~/.config/sway/config`にコピーすれば動きます。
そうでないならば、サンプルのコンフィグファイルを`~/.config/sway/config`にコピーしてください。
サンプルのコンフィグファイルは基本的には`/etc/sway/config`にあります。
`man 5 sway`で各種設定について確認できます。

## 実行

`sway`をTTYから実行してください。いくつかのDesktopManagerはSwayからサポートされていませんが、動く場合もあります(gdmは特にSwayと相性が良いそうです)。

