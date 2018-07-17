## Logging

Use `sway_log(importance, fmt, ...)` to log. The following importances are
available:

* `WLR_DEBUG`: Debug messages, only shows with `sway -d`
* `WLR_INFO`: Informational messages
* `WLR_ERROR`: Error messages

`sway_log` is a macro that calls `_sway_log` with the current filename and line
number, which are written into the log with your message.

## Assertions

In the compositor, assertions *must not* be fatal. All error cases must be
handled as gracefully as possible - crashing the compositor will make the user
lose all of their work.

Use `sway_assert(condition, fmt, ...)` to perform an assertion. This returns
`condition`, which you must handle if false. An error will be logged if the
assertion fails.

Outside of the compositor (swaymsg, swaybar, etc), using `assert.h` is
permitted.
