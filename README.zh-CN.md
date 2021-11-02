# sway

sway 是和 [i3](https://i3wm.org/) 兼容的 [Wayland](http://wayland.freedesktop.org/) compositor.
阅读 [FAQ](https://github.com/swaywm/sway/wiki). 加入 [IRC
频道](https://web.libera.chat/gamja/?channels=#sway) (#sway on
irc.libera.chat).

## 发布签名

发布是以 [E88F5E48](https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48) 签名
并发布在 [GitHub](https://github.com/swaywm/sway/releases).

## 安装

### 从软件包中

Sway 在很多发行版中可用. 尝试在你的发行版中安装 "sway" 包.
如果这不可用, 请到 [此 wiki 页](https://github.com/swaywm/sway/wiki/Unsupported-packages)
检查针对你的发行版关于安装的信息.

如果你有兴趣给你的发行版打包 sway, 停下来到 IRC 频道或者发邮件至 sir@cmpwn.com 获取建议.

### 从源代码编译

安装依赖:

* meson \*
* [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (可选的: system tray)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (可选的: man pages) \*
* git \*

_\*编译时依赖_

运行这些命令:

    meson build
    ninja -C build
    sudo ninja -C build install

在没有 logind 的系统上, 你需要给 sway 二进制设置 suid:

    sudo chmod a+s /usr/local/bin/sway

Sway 将会在启动后尽快丢掉 root 权限.

## 配置

如果你已经在使用 i3, 接下来复制你的 i3 配置到 `~/.config/sway/config`
它可以直接工作. 或者, 复制样本配置文件到
`~/.config/sway/config`. 它通常位于 `/etc/sway/config`.
运行 `man 5 sway` 获取关于配置的信息.

## 运行

从 TTY 中运行 `sway` . 某些显示管理器可能会工作但并不被 sway 支持
(已知的 gdm 工作得非常好).
