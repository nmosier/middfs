# MiddFS
## A Peer-to-`basin`-to-Peer Distributed Filesystem for Middlebury College
## Nicholas Mosier and Tommaso Monaco

# Table of Contents
1. [Requirements](#requirements)
1. [Installation](#installing-middfs)
1. [Configuration](#configuring-middfs)
1. [Usage](#using-middfs)
1. [Source code](#the-middfs-source-code)
1. [Bugs, Errors, Questions](#bugs-errors-questions)

# Requirements
MiddFS can run on any UNIX-based system (theoretically). It _has_ been tested on and is known to work on macOS, Linux, and FreeBSD.

# Installing MiddFS
You can install MiddFS on any UNIX-based system (macOS, Linux, FreeBSD, etc.). Its only requirement is that [FUSE](https://github.com/libfuse/libfuse) be installed
and that the operating system be POSIX-compliant.

## Installing FUSE
FUSE is a library that allows custom filesystems to run in userspsace.
Here are platform-specific instructions for installing FUSE:
- [macOS](#installing-fuse-on-macos)
- [Linux](#installing-fuse-on-linux)
- [FreeBSD](#installing-fuse-on-freebsd)
After installing fuse, you _will_ need to reboot.

### Installing FUSE on macOS

If you have Homebrew, simply run

```bash
$ brew cask install osxfuse
```
Homebrew may list some caveats, e.g. you might need to enable OSXFUSE's kernel extension in System Preferences -> Security & Privacy -> General.

Otherwise, you can directly download the package from [OSXFUSE's GitHub Releases page](https://github.com/osxfuse/osxfuse/releases).

### Installing FUSE on Linux

FUSE is installed by default on all major Linux distributions, so you shouldn't need to install it. If you do for some reason, you can install it through your system's package manager.

### Installing FUSE on FreeBSD

You can install FUSE using `pkg`, the built-in package manager for FreeBSD. The FUSE packafe is named `fusefs3`; to install it, type
```bash
#  pkg install fusefs3
```

### Installing FUSE on Windows
You might be able to install FUSE on Windows if you try hard enough. We're not providing instructions on how to do that here, though.
Anyway, MiddFS has _not_ been tested on Windows.

## Compiling MiddFS
To compile MiddFS, simply run the following:
```bash
$ git clone https://github.com/nmosier/middfs.git
$ cd middfs/src
$ make client
```
Compilation should produce an executable at `./client/middfs-client`.
This is the MiddFS client program that will let you connect to an existing MiddFS network.

_Optional_: To put `middfs-client` on your path, simply run
```bash
$ cp middfs/client-sever /usr/local/bin/
```

# Configuring MiddFS
Before mounting MiddFS, you will need to create a configuration file. You can save it anywhere, but the recommended path is `~/.middfs.conf` (in your home directory).
The MiddFS configuration file requires the following parameters:
```
username=<string>
homepath=<path>
mountpoint=<path>
serverip=<dotted quad>
serverport=<integer>
localport=<integer>
```

###### Username
Your username can be anything, as long as it doesn't have any spaces in it.
The server uses your username as the name of your MiddFS user directory.

###### Home Path
Your home path specifies what directory on your local filesystem that you want to 
make public on MiddFS.
Do **_not_** set this to be any directory that contains sensitive information or file data that you aren't willing to lose and don't have a backup of.
Finally, use the _full path_ of your home directory in the configuration file, with no shortcuts like `~`.

###### Mountpoint
The `mountpoint` configuration variable specifies the mountpoint of the MiddFS filesystem. After mounting MiddFS, the contents of other client's public folders (and your own) will be visible inside of this directory.
You can use pretty much any directory for MiddFS; some logical ones might be `/mnt/middfs` (Linux/FreeBSD) or `/Volumes/middfs` (macOS) or even `~/middfs` (where you substitute `~` for your actual home path).
Note that whatever path you choose for your mountpoint must exist _and_ be a directory, preferably empty before mounting.

###### Server IP and Port
Note that whitespace _does_ matter.
If you are using MiddFS on Middlebury campus, the MiddFS server will be running on `basin`, so set
```
serverip=140.233.20.6
serverport=4321
```
(If `basin`'s IP changes, simply obtain it using the command `$ host basin.middlebury.edu`.)

###### Localport
The `localport` configuration variable specifies on which port your MiddFS client should listen for and accept incoming requests.
It is recommended that you select a localport above `1024`, since those don't require superuser permissions and probably won't already be in use if you choose one at random.
Some nice localport numbers are `4001` or `4999` because they are both prime, which makes MiddFS respond to incoming requests faster. (Not really...)

### Example Configuration File
Here is an example configuration file:
```
username=nmosier
homepath=/Users/nmosier/public
mountpoint=/Users/nmosier/middfs
serverip=140.233.20.6
serverport=4321
localport=4999
```

# Using MiddFS

Just like any other filesystem, you need to mount MiddFS before you can use it.

### Mounting MiddFS
Once you have completed the installation and configuration of MiddFS, you can mount it with the following command:
```
middfs-client --conf=<path/to/middfs/conf> <mountpoint>
```
where `<path/to/middfs/conf>` is the absolute path to your MiddFS configuration file
and `<mountpoint>` is the same mountpoint you set in your configuration file (yes, this is annoying and redundant).

After successfully mounting MiddFS, a list of directories with the usernames of other connected clients (including you) should appear under your mountpoint. These should act (mostly) like regular directories, but they aren't actually on your hard drive! Any operation you make on these files results in your MiddFS client contacting the server, retrieving the file information from another MiddFS client, and then relaying the results back to your operating system.
You should be able to create new files, read to and write from files, enter other clients' directories, and more. 

### Unmounting MiddFS

The steps to unmount MiddFS vary platform-to-platform. On macOS, you should be able to right-click on the MiddFS mountpoint in Finder and then select "Eject".
On any UNIX-based operating system, you can always unmount the MiddFS filesystem using
```
umount <mountpoint>
```
where `<mountpoint>` is the path to the MiddFS mountpoint. If that doesn't work, you can always kill the middfs-client process with
```
pkill middfs-client
```

# The MiddFS Source Code

The source code to MiddFS is located under `src/`.
There are three main subdirectories:
- `src/`
    - `lib/`
    - `client/`
    - `server/`

The `lib/` subdirectory contains shared code used in both the MiddFS server and the MiddFS client.
The code in `lib/` is compiled into a static library called `lib/middfs-lib.a`; run
```bash
$ make lib
```
inside `src/` to build the library by itself.

The `client/` subdirectory contains code specific to the MiddFS client. To make the client, run the command
```bash
$ make client
```
inside `src/`. This produces the executable `src/client/middfs-client`.

The `server/` subdirectory contains code specific to the MiddFS server. To make the server, run the command
```bash
$ make server
```
inside `src/`. This produces the executable `src/server/middfs-server`.

# Tips

- MiddFS is generally faster for command-line utilities than for GUI applications (because they are more decisive in their file operations).
- Having MiddFS open in your file explorer can cause many unnecessary idle file operations and reduce performance in other processes using files on MiddFS. (This problem will be mitigated by optimizations to MiddFS.)

# Bugs, Errors, Questions

If you'd like to see a feature added to MiddFS, please create an issue tagged as a "feature" on [MiddFS's GitHub repository](https://github.com/nmosier/middfs).

If you encounter any errors during compilation, please report them as issues on [MiddFS's GitHub repository](https://github.com/nmosier/middfs).

