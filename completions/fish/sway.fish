# sway(1) completion

complete -f -c sway
complete -c sway -s h -l help --description "Show help message and quit."
complete -c sway -s c -l config --description "Specifies a config file." -r
complete -c sway -s C -l validate --description "Check the validity of the config file, then exit."
complete -c sway -s d -l debug --description "Enables full logging, including debug information."
complete -c sway -s v -l version --description "Show the version number and quit."
complete -c sway -s V -l verbose --description "Enables more verbose logging."
complete -c sway -l get-socketpath --description "Gets the IPC socket path and prints it, then exits."

