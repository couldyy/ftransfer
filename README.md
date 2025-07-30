# ftransfer
Simple program to transfer files over network using TCP/IP protocol

## Build
In order to build, clone repo and do:
```
make
```
and for windows:
```
make windows
```
You can change comiler, see `Makefile`

## Usage
Comand format is the following:
```
ftransfer [mode] [ip] [port]
```
Modes are: `-s` for sending files, `-c` for receiving

So formats for sending and receiving are:
```
ftransfer -s [start server on IP] [PORT]    // for sending
ftransfer -c [receive from IP] [PORT]       // for receiving
```

Run and follow programs instrucions ;)
