# Sway

English - 日本語 - Deutsch - Ελληνικά - Français - Español - Українська - Italiano - Nederlands

“**S**irCompwn’s **Way**land compositor” is een **werk-in-uitvoering** zijnde i3-compatible [Wayland](http://wayland.freedesktop.org/) compositor. Lees Voor meer informatie a.u.b. the [FAQ](https://github.com/SirCmpwn/sway/wiki) of bezoek het IRC kanaal (#sway op irc.freenode.net). 

![sway](https://camo.githubusercontent.com/6e869959b9de17be94823d9423ba7bbc0adeff09/68747470733a2f2f73722e68742f494364352e706e67)

Mocht u de ontwikkeling van Sway willen steunen, dan kan er een bijdrage geleverd worden bij [SirCmpwn's Patreon Pagina](https://patreon.com/sircmpwn) of d.m.v. een zgn. [bounty](https://github.com/SirCmpwn/sway/issues/986) t.b.v. het ondersteunen van specifieke functies binnen Sway. Het staat Iedereen vrij om een bounty te claimen voor elke gewenste functie, terwijl ondersteuning via Patreon bevorderlijker is voor het ontwikkelen van Sway als geheel. 

# Tekenen van versies

Nieuwe versies worden getekend met [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A) en gepubliceerd op [GitHub](https://github.com/SirCmpwn/sway/releases).

# Huidige Status

* [Ondersteuning van i3 functies](https://github.com/SirCmpwn/sway/issues/2)
* [Ondersteuning van IPC functies](https://github.com/SirCmpwn/sway/issues/98)
* [Ondersteuning van i3bar functies](https://github.com/SirCmpwn/sway/issues/343)
* [Ondersteuning van i3-gaps functies](https://github.com/SirCmpwn/sway/issues/307)
* [Ondersteuning van veiligheidsfuncties](https://github.com/SirCmpwn/sway/issues/984)
    
# Installatie

## Voorverpakte Versies

Sway is voorverpakt in meerdere Linux distributies. Het is mogelijk eerst proberen ‘sway’ te installeren in de door u gebruike distributie. Indien dit niet aanwezig is kunt u [deze Wiki pagina](https://github.com/SirCmpwn/sway/wiki/Unsupported-packages) bezoeken voor meer informatie over de installatieprocedure voor uw distributie. 

Als u interesse heeft in het voorverpakken van Sway voor uw distributie, bezoek dan het IRC kanaal of stuur een email (in het Engels) naar sir@cmpwn.com voor advies.

# Broncompilatie 

Noodzakelijke dependencies:

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
* gdk-pizbuf2 *
* pam ** 
* imagemagick (noodzakelijk voor fotografie met swaygrab)
* ffmpeg (noodzakelijk voor video-opname met swaygrab)

*_Alleen noodzakelijk voor swaybar, swaybg en swaylock_

**_Alleen noodzakelijk voor swaylock_

Voer de onderstaande commando's uit:

	mkdir build 
	cd build 
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_SYSCONFDIR=/etc .. 
	make 
	sudo make install
	
Op systemen met logind dient de binary op voorhand gemodificeerd te worden:

	sudo setcap cap_sys_ptrace=eip /usr/local/bin/sway
	sudo setcap cap_sys_tty_config=eip /usr/local/bin/sway

Op systemen zonder logind dient men het onderstaande uit te voeren:

	sudo chmod a+s /usr/local/bin/sway

# Configuratie

Indien u reeds gebruikmaakt van i3, kunt u simpelweg uw oorspronkelijke i3 configuratie kopiëren naar ```~/.config/sway/config```, waarna het automatisch werkt. Kopieer in andere gevallen het voorbeeldconfiguratiebestand naar ```~/.config/sway/config```. Dit bestand kunt u vinden in ```/etc/sway/config```. Run ```man 5 sway``` voor meer informatie aangaande de configuratie van Sway. 

# Starten

Voer ```sway``` uit vanuit een TTY. Sommige Display Managers zouden kunnen werken maar worden niet direct ondersteund door Sway (van gdm is het bekend dat het redelijk goed werkt.)

# Ondersteuning in het Nederlands

SleepyMario biedt ondersteuning in het Nederlands op IRC en GitHub, maar op onregelmatige tijdstippen. Laat aub. een berichtje achter en hij zal u z.s.m te woord staan.  

Stuur een email naar [SleepyMario](theonesleepymario@gmail.com) voor suggesties wat betreft deze vertaling. 

