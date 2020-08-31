---
name: Bugs
about: Crashes and other bugs which doesn't occur in Sway upstream
labels: 'bug'

---

### Please read the following before submitting:
- Make sure the issue is with one of the additional features of sway-borders or one which does not occur in sway upstream.

### Please fill out the following:
- **Sway Version:**
  - `swaymsg -t get_version` or `sway -v`

- **Debug Log:**
  - Run `sway -d 2> ~/sway.log` from a TTY and upload it to a pastebin, such as gist.github.com.
  - This will record information about sway's activity. Please try to keep the reproduction as brief as possible and exit sway.

- **Configuration File:**
  - Please try to produce with the default configuration.
  - If you cannot reproduce with the default configuration, please try to find the minimal configuration to reproduce.
  - Upload the config to a pastebin such as gist.github.com.

- **Stack Trace:**
  - This is only needed if sway crashes.
  - If you use systemd, you should be able to open the coredump of the most recent crash with gdb with
    `coredumpctl gdb sway` and then `bt full` to obtain the stack trace.
  - If the lines mentioning sway or wlroots have `??` for the location, your binaries were built without debug symbols. Please compile both sway and wlroots from source and try to reproduce.

- **Description:**
  - The steps you took in plain English to reproduce the problem.
