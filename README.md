# Multithreaded-CPP-Backup-Server

This server runs in stateless protocol using Boost.

### Installation

Use `brew` package manager to download the latest `cmake` `make` and `boost` .

- If you don't have `brew` installed, open `Terminal` and run the following commands:

      xcode-select --install        # developer tools are required
      /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

* After you have `brew` installed:

        brew install boost cmake make 


* Otherwise, all you need to do is to run the Makefile which will execute the CMakefile

      cd PATH/TO/SERVER
      make

* In separate terminals, launch both clients and server

      # each in sepreate termianal
      cd ../server/ -> python3 client.py
      cd server/   -> ./server

### WINDOWS

You can use the same steps as above since `vcpkg` is a microsoft product or you can download all the necessary libs using Visual Studio.
