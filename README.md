My Shell (LALO Shell)
a very lightweight, functional unix shell written in C. This project demonstrates core systems programming concepts including process creation, file descriptor manipulation, and inter-process-communication through piping.
Features:
**Command Execution**: Runs any standard Unix command using the `exec` family of system calls.
**Piping**: Supports complex pipelines (e.g., `ls -l | grep ".c" | wc -l`).
**I/O Redirection**: 
  - Input redirection (`<`)
  - Output redirection (`>`)
  - Append redirection (`>>`)
**History**: Integrated with `GNU Readline` to provide arrow-key navigation through past commands. History is saved across sessions in `~/.shell_history.txt`.
**Dynamic Prompt**: Displays `username@hostname:current_directory>`.