# sway

sway არის [i3]-თავსებადი [Wayland]-ის კომპოზიტორი. მეტი ინფორმაციისთვის იხილეთ 
[FAQ]. დაუკავშირდით [IRC არხს][IRC channel] \(#sway irc.libera.chat-ზე).

## გამოშვების ხელმოწერები

გამოშვებები ხელმოწერილია [E88F5E48]-ით და გამოქვეყნებულია [GitHub-ზე][GitHub releases].

## ინსტალაცია

### რეპოზიტორიიდან

Sway არის ხელმისაწვდომი ბევრი დისტრიბუტაციისთვის. ცადეთ "sway" პაკეტის ინსტალაცია თქვენთვის.

### კოდის კომპილაცია

იხილეთ [ეს ვიკი გვერდი][Development setup] თუ გინდათ რომ ააწყოთ sway და wlroots სატესტოდ ან დეველოპმენტისთვის.

დააინსტალირეთ დამოკიდებულებები:

* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* pcre2
* json-c
* pango
* cairo
* gdk-pixbuf2 (ასევე არჩევითია: system tray)
* [scdoc] (ასევე არჩევითია: man pages) \*
* git (ასევე არჩევითია: version info) \*

_\* Compile-time dep_

გაუშვით ეს ბრძანებები:

    meson build/
    ninja -C build/
    sudo ninja -C build/ install

## კონფიგურაცია

თუ უკვე იყენებთ i3-ს, მაშინ დააკოპირე i3 კონფიგურაცია და ჩასვი `~/.config/sway/config` 
და უპრობლემოდ იმუშავებს პირდაპირ. წინააღმდეგ შემთხვევაში კონფიგურაციის ნიმუში ჩააკოპირეთ აქ: `~/.config/sway/config`. კომპიგურაციის ნიმუში ხშირ შემთხვევაში არის `/etc/sway/config`.
გაუშვი `man 5 sway` კონპიგურაციაზე ინფორმაციის მისაღებად.

## გაშვება

გაუშვი `sway` TTY-ისთვის. ზოგიერთმა ლოგინ მენეჯერმა შეიძლება იმუშავოს, მაგრამ არ
არის მხარდაჭერილი sway-სგან (როგორც წესი კარგად მუშაობს gdm).

[i3]: https://i3wm.org/
[Wayland]: http://wayland.freedesktop.org/
[FAQ]: https://github.com/swaywm/sway/wiki
[IRC channel]: https://web.libera.chat/gamja/?channels=#sway
[E88F5E48]: https://keys.openpgp.org/search?q=34FF9526CFEF0E97A340E2E40FDE7BE0E88F5E48
[GitHub releases]: https://github.com/swaywm/sway/releases
[Development setup]: https://github.com/swaywm/sway/wiki/Development-Setup
[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
[scdoc]: https://git.sr.ht/~sircmpwn/scdoc
