## Logging

Use `sway_log(importance, fmt, ...)` to log. The following importances are
available:

* `L_DEBUG`: Debug messages, only shows with `sway -d`
* `L_INFO`: Informational messages
* `L_ERROR`: Error messages

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

## Building against a local wlc

1. Build wlc as described [here](https://github.com/Cloudef/wlc#building)
2. Inside your sway source folder, tell `cmake` to use your local version of wlc:

```bash
cmake \
    -DWLC_LIBRARIES=path/to/wlc/target/src/libwlc.so \
    -DWLC_INCLUDE_DIRS=path/to/wlc/include .
```
