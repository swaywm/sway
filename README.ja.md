# sway [![](https://api.travis-ci.org/swaywm/sway.svg)](https://travis-ci.org/swaywm/sway) [![Donate with fosspay](https://drewdevault.com/donate/static/donate-with-fosspay.png)](https://drewdevault.com/donate?project=4)

"**S**irCmpwn's **Way**land compositor"は**開発中**の
i3互換な[Wayland](http://wayland.freedesktop.org/)コンポジタです。
[FAQ](https://github.com/swaywm/sway/wiki)も合わせてご覧ください。
[IRC チャンネル](http://webchat.freenode.net/?channels=sway&uio=d4) (#sway on irc.freenode.net)もあります。

**注意**: Swayは現在*凍結中*であり、Swayとwlrootsの統合が完了するまで、新たな機能は追加されません。バグフィックスは行われます。詳しくは[この記事](https://drewdevault.com/2017/10/09/Future-of-sway.html)をご覧ください。wlrootsとの統合状況については、[このチケット](https://github.com/swaywm/sway/issues/1390)をご覧ください。

[![](https://sr.ht/ICd5.png)](https://sr.ht/ICd5.png)

Swayの開発を支援したい場合は、[SirCmpwnのPatreon](https://patreon.com/sircmpwn)や、特定の機能に対する[報奨金のページ](https://github.com/swaywm/sway/issues/986)から寄付ができます。誰でも報奨金を請求できますし、自分の欲しい機能に報奨金を懸ける事も出来ます。またSwayのメンテナンスを支援するには、Patreonがより有用です。

## 日本語サポート
SirCmpwnは、日本語でのサポートをIRCとGitHubで行います。タイムゾーンはUTC-4です。

## リリースの署名

Swayのリリースは[B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)で署名され、[GitHub](https://github.com/swaywm/sway/releases)で公開されています。

## 開発状況

- [i3の機能のサポート](https://github.com/swaywm/sway/issues/2)
- [IPCの機能のサポート](https://github.com/swaywm/sway/issues/98)
- [i3barの機能のサポート](https://github.com/swaywm/sway/issues/343)
- [i3-gapsの機能のサポート](https://github.com/swaywm/sway/issues/307)
- [セキュリティ機能](https://github.com/swaywm/sway/issues/984)

## インストール

### パッケージから

Swayは沢山のディストリビューションで提供されています。"sway"パッケージのインストールを試してください。パッケージが存在しない場合は、[このページ](https://github.com/swaywm/sway/wiki/Unsupported-packages)で、あなたのディストリビューションでのインストールに関する情報を調べてください。

あなたのディストリビューションにSwayのパッケージを提供したい場合は、SwayのIRCチャンネルを訪れるか、sir@cmpwn.comにメールを送り、相談してください。

### ソースコードからコンパイル

次の依存パッケージをインストールしてください:

* meson
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

_\*swaybar,swaybg,swaylockでのみ必要です_

_\*\*swaylockでのみ必要です_

次のコマンドを実行してください:

    meson build
    ninja -C build
    sudo ninja -C build install

logindを使用しているシステムでは、バイナリにいくつかのケーパビリティを設定する必要があります:

    sudo setcap "cap_sys_ptrace,cap_sys_tty_config=eip" /usr/local/bin/sway

logindを使用していないシステムでは、バイナリにsuidを設定する必要があります:

    sudo chmod a+s /usr/local/bin/sway

## 設定

既にi3を使用している場合は、i3の設定ファイルを`~/.config/sway/config`にコピーすれば動きます。そうでない場合は、サンプルの設定ファイルを`~/.config/sway/config`にコピーしてください。サンプルの設定ファイルは、通常`/etc/sway/config`にあります。`man 5 sway`を実行することで、設定に関する情報を見ることができます。

## 実行

`sway`をTTYから実行してください。いくつかのディスプレイマネージャは動くかもしれませんが、Swayからサポートされていません(gdmは非常に良く動作することが知られています)。

