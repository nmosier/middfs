# MiddFS
## A Peer-to-`basin`-to-Peer Distributed Filesystem for Middlebury College
## Nicholas Mosier and Tommaso Monaco

# Table of Contents
1. [Installation](#installation)

# Installation
You can install MiddFS on any UNIX-based system (macOS, Linux, FreeBSD, etc.). Its only requirement is that [FUSE](https://github.com/libfuse/libfuse) be installed. 

## Installing FUSE
After installing fuse, you _will_ need to reboot.

### Installing FUSE on macOS
If you have Homebrew, simply run

```brew cask install osxfuse```

Homebrew may list some caveats, e.g. you might need to enable OSXFUSE's kernel extension in System Preferences -> Security & Privacy -> General.
