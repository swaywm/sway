Please include the following four components in your bug report: sway version, debug log, configuration (if applicable), and an explanation of steps taken to reproduce the issue.

Obtain your version like so:

    swaymsg -t get_version

If this doesn't work, use:

    sway -v

* Sway Version:

Obtain a debug log like so:

    sway -d 2> ~/sway.log

This will record information about sway's activity when it's running. Briefly reproduce your problem and exit sway.  When preparing a debug log, brevity is important - start up sway, do the minimum work necessary to reproduce the error, then close sway.

Upload the debug log to a pastebin service such as [gist.github.com](https://gist.github.com), and link to it below.

* Debug Log:

You should try to reproduce the issue with the default configuration. If you cannot, please reproduce with a minimal configuration, upload the config to a pastebin service, and link to it below.

* Configuration File:

Finally, explain the steps you took in plain English to reproduce the problem below.
