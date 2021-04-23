# sway

Swayは[i3](https://i3wm.org/)互換な[Wayland](http://wayland.freedesktop.org/)コンポジタです。
[FAQ](https://github.com/swaywm/sway/wiki)も合わせてご覧ください。
[IRC チャンネル](http://webchat.freenode.net/?channels=sway&uio=d4) (irc.freenode.netの#sway)もあります。

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

## 日本語サポート

SirCmpwnは、日本語でのサポートをIRCとGitHubで行います。タイムゾーンはUTC-4です。

## リリースの署名

Swayのリリースは[B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)で署名され、[GitHub](https://github.com/swaywm/sway/releases)で公開されています。

## インストール

### パッケージから

Swayは沢山のディストリビューションで提供されています。"sway"パッケージのインストールを試してください。パッケージが存在しない場合は、[このページ](https://github.com/swaywm/sway/wiki/Unsupported-packages)で、あなたのディストリビューションでのインストールに関する情報を調べてください。

あなたのディストリビューションにSwayのパッケージを提供したい場合は、SwayのIRCチャンネルを訪れるか、sir@cmpwn.comにメールを送り、相談してください。

### ソースコードからコンパイル

次の依存パッケージをインストールしてください:

* meson \*
* [wlroots](https://github.com/swaywm/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (システムイコンで必要です)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (manで必要です) \*
* git \*

_\*コンパイルの時_

次のコマンドを実行してください:

    meson build
    ninja -C build
    sudo ninja -C build install

logindを使用していないシステムでは、バイナリにsuidを設定する必要があります:

    sudo chmod a+s /usr/local/bin/sway

swayは起動後、すぐにroot許可を落とします。

## 設定

既にi3を使用している場合は、i3の設定ファイルを`~/.config/sway/config`にコピーすれば動きます。そうでない場合は、サンプルの設定ファイルを`~/.config/sway/config`にコピーしてください。サンプルの設定ファイルは、通常`/etc/sway/config`にあります。`man 5 sway`を実行することで、設定に関する情報を見ることができます。

## 実行

`sway`をTTYから実行してください。いくつかのディスプレイマネージャは動くかもしれませんが、Swayからサポートされていません(gdmは非常に良く動作することが知られています)。

