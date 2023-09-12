# sway

sway 是和 [i3](https://i3wm.org/) 兼容的 [Wayland](http://wayland.freedesktop.org/) compositor。
 [查看FAQ](https://github.com/swaywm/sway/wiki)/ [加入IRC频道](https://web.libera.chat/gamja/?channels=#sway) (#sway on irc.libera.chat)

## 发行签名

每个发行版都以 [E88F5E48](https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48) 的密钥签名并发布在 [GitHub](https://github.com/swaywm/sway/releases)上。

## 安装

### 从包管理器安装

Sway 在很多发行版中可用。请尝试在你的发行版中安装 `sway` 。

### 从源码编译

如果想要构建最新版sway和wlroots用以测试和开发，请查看 [此wiki页面](https://github.com/swaywm/sway/wiki/Development-Setup)

安装如下依赖:

* meson \*
* [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (可选的: system tray)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (可选: man pages) \*
* git \*

_\*编译时依赖_

运行如下命令:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

## 配置

如果你已经在使用i3，直接复制i3配置文件到 `~/.config/sway/config`，这是开箱即用的。或者，你可以复制配置样例到`~/.config/sway/config`。它通常位于 `/etc/sway/config`。
运行 `man 5 sway` 获取关于配置的更多信息。

## 运行

从 TTY 中运行 `sway`。 某些显示管理器（Display Manager）也许可以工作但不被 sway 支持。
(已知 gdm 工作得非常好)。
