# tcp-socket-encrypted-chat
A simple command line chat application built on TCP sockets with end-to-end encryption.

## Prerequisites

This app works only on linux operating systems. 

You'll also need the [cryptodev-linux](http://cryptodev-linux.org) library or support for `/dev/crypto`. 

## Compilation

`make`

## Usage

`./chatter <server|client> <shared_key> <port> <crypto_file_path> <hostname>`
