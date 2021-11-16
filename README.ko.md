# sway

sway는 [i3](https://i3wm.org/)-호환 [Wayland](http://wayland.freedesktop.org/) 컴포지터입니다.
[FAQ](https://github.com/swaywm/sway/wiki)를 읽어보세요. [IRC 채널](https://web.libera.chat/gamja/?channels=#sway) (#sway on irc.libera.chat)도 있습니다.

## 릴리즈 서명

릴리즈는 [E88F5E48](https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48)에서 서명되고,
[GitHub에서](https://github.com/swaywm/sway/releases) 공개되고 있습니다.

## 설치

### 패키지를 통한 설치

Sway는 많은 배포판에서 이용할 수 있습니다. "sway" 패키지를 설치해 보세요.
만약 없다면, [위키 페이지](https://github.com/swaywm/sway/wiki/Unsupported-packages)를 확인하세요.
해당 배포판에 대한 설치 정보를 확인할 수 있습니다.

당신의 배포판에 sway 패키지를 제공하고 싶다면,
IRC 채널을 방문하거나 sir@cmpwn.com으로 이메일을 보내 상담 받으세요.

### 소스를 통한 컴파일

다음 의존 패키지들을 설치해 주세요:

* meson \*
* [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)
* wayland
* wayland-protocols \*
* pcre
* json-c
* pango
* cairo
* gdk-pixbuf2 (선택: system tray)
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (선택: man pages) \*
* git \*

_\*컴파일 떄 필요_

다음 명령을 실행하세요:

    meson build
    ninja -C build
    sudo ninja -C build install

logind를 사용하고 있지 않는 시스템에서는, 바이너리에 suid를 설정할 필요가 있습니다:

    sudo chmod a+s /usr/local/bin/sway
    
Sway는 시작 후에 root 권한을 drop할 것 입니다.

## 설정

i3를 이미 사용 중이라면, i3 config을 `~/.config/sway/config`로 복사하세요.
아니면, 샘플 구성 파일을 '~/.config/sway/config'에 복사할 수도 있습니다.
일반적으로 "/etc/sway/config"에 위치해 있습니다.
설정에 대한 정보를 보려면 "man 5 sway"를 실행하세요.

## 실행

TTY에서 `sway`를 실행하세요. 일부 display manager는 작동하지만, sway로 부터 지원되지 않습니다(gdm은 상당히 잘 작동한다고 알려져 있습니다).
