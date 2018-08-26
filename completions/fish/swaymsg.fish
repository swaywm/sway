# swaymsg(1) completion

complete -c swaymsg -s h -l help --description "Show help message and quit."
complete -c swaymsg -s q -l quiet --description "Sends the IPC message but does not print the response from sway."
complete -c swaymsg -s r -l raw --description "Use raw output even if using tty."
complete -c swaymsg -s s -l socket --description "Use the specified socket path. Otherwise, swaymsg will ask where the socket is (which is the value of $SWAYSOCK, then of $I3SOCK)."
complete -c swaymsg -s t -l type --description "Specify the type of IPC message."
complete -c swaymsg -s v -l version --description "Print the version (of swaymsg) and quit."
