# sway

[English][en] - [عربي][ar] - [Azərbaycanca][az] - [Česky][cs] - [Deutsch][de] - [Dansk][dk] - [Español][es] - [Français][fr] - [ქართული][ge] - [Ελληνικά][gr] - [हिन्दी][hi] - [Magyar][hu] - [فارسی][ir] - [Italiano][it] - [日本語][ja] - [한국어][ko] - [Nederlands][nl] - [Norsk][no] - [Polski][pl] - [Português][pt] - [Română][ro] - [Русский][ru] - [Српски][sr] - [Svenska][sv] - **ไทย** - [Türkçe][tr] - [Українська][uk] - [中文-简体][zh-CN] - [中文-繁體][zh-TW]

sway คือ compositor สำหรับ [Wayland] ที่เข้ากันได้กับ [i3] อ่าน [FAQ] ได้ที่นี่
เข้าร่วม[ช่อง IRC] \(#sway on irc.libera.chat)

## Release Signatures

รีลีสถูกลงนามด้วย [E88F5E48] และเผยแพร่[บน GitHub][GitHub releases]

## Installation

### From Packages

Sway มีให้ใช้งานในดิสทริบิวชันหลายตัว ลองติดตั้งแพ็คเกจ "sway" สำหรับ
ดิสทริบิวชันของคุณ

### Compiling from Source

ตรวจสอบ[หน้า wiki นี้][Development setup] หากคุณต้องการสร้าง HEAD ของ
sway และ wlroots สำหรับการทดสอบหรือพัฒนา

ติดตั้ง dependencies:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (ตัวเลือก: รูปแบบภาพเพิ่มเติมสำหรับ system tray)
* [swaybg] (ตัวเลือก: วอลเปเปอร์)
* [scdoc] (ตัวเลือก: man pages) \*
* git (ตัวเลือก: ข้อมูลเวอร์ชัน) \*

_\* Compile-time dep_

รันคำสั่งเหล่านี้:

    meson setup build/
    ninja -C build/
    sudo ninja -C build/ install

## Configuration

หากคุณใช้ i3 อยู่แล้ว ให้คัดลอกคอนฟิก i3 ของคุณไปที่ `~/.config/sway/config` และ
มันจะใช้งานได้ทันที มิฉะนั้น ให้คัดลอกไฟล์คอนฟิกตัวอย่างไปที่
`~/.config/sway/config` โดยปกติจะอยู่ที่ `/etc/sway/config`
รัน `man 5 sway` เพื่อดูข้อมูลเกี่ยวกับการตั้งค่า

## Running

รัน `sway` จาก TTY หรือจาก display manager

[en]: https://github.com/swaywm/sway#readme
[ar]: README.ar.md
[az]: README.az.md
[cs]: README.cs.md
[de]: README.de.md
[dk]: README.dk.md
[es]: README.es.md
[fr]: README.fr.md
[ge]: README.ge.md
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
[sr]: README.sr.md
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
[swaybg]: https://github.com/swaywm/swaybg/
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
