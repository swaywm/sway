# sway

sway 是和 [i3](https://i3wm.org/) 兼容的 [Wayland](http://wayland.freedesktop.org/) compositor。
 [阅读FAQ](https://github.com/swaywm/sway/wiki)/ [加入IRC频道](https://web.libera.chat/gamja/?channels=#sway) (#sway on irc.libera.chat)

## 发行签名

每个发布版本都以 [E88F5E48](https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48) 的密钥签名并发布在 [GitHub](https://github.com/swaywm/sway/releases)上。

## 安装

### 从包管理器安装

Sway 在很多发行版中可用。请尝试在你的发行版中安装 `sway` 。
如果不行, 请查看 [此 wiki 页面](https://github.com/swaywm/sway/wiki/Unsupported-packages) 检查针对你的发行版的安装信息。

如果你有兴趣给你的发行版打包 sway, 请查看 IRC 频道或者发送邮件至 sir@cmpwn.com 获取建议。

### 从源码编译

请安装如下依赖:

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

在没有 logind 的系统上, 你需要给 sway 二进制文件设置 suid:

    sudo chmod a+s /usr/local/bin/sway

启动后，Sway会尽快放弃root权限。

## 配置

如果你已经在使用 i3, 你可以复制你的 i3 配置文件到 `~/.config/sway/config`。
这可以直接工作。或者，你可以复制配置样例到`~/.config/sway/config`。它通常位于 `/etc/sway/config`。
运行 `man 5 sway` 获取关于配置的信息。

## 运行

从 TTY 中运行 `sway`。 某些Display Manager也许可以工作但不被 sway 支持。
(已知 gdm 工作得非常好)。
