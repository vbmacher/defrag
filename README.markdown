Introduction
------------

 The program is used for defragmentation of disk image files with FAT32 file system. It is split into several
 modules, from that each can  work with its "problem domain". These modules cooperate with each other.
 My aim was to hide as many functions as it is possible, in order I would achieve the biggest "encapsulation"
 (even if the C language is not object oriented). It allowed simplier program debuging.
 
 While writting this project I was thinking on the motto: Keep It Simple Stupid (KISS).
 In case of any questions, please write me on my email pjakubco@gmail.com

Program compiling
-----------------

 In order to simplify the compilation I have used the make program and I have created `Makefile` file. To compile
 the source code into binary file, type: `make`. The executable file is called `defrag`.
 
License
-------

This project is released under GNU GPL v2 license.

System requirements
-------------------

 The software needs any distribution of UNIX/LINUX that contains tools: `gcc`, `make`, `xgettext`, and `sed`.

 The `xgettext` program is used for automatical retrieving of strings that will be localized and therefore multilanguage
 is supproted. Additional translation and creation of binary localization file is needed to perform manually. Currently,
 there are supported 2 languages: English and Slovak.

Testing
-------

Empty FAT32 images can be created as follows: `mkfs.msdos -v -C -F 32 -n TestFAT32 fat32.img 1000`. The arguments means:

* `-v` - detailed information about progress
* `-C` - create disk image
* `-F <%d>` - FAT type (12, 16, 32)
* `-n <%s>` - name of the partition (not necessary)
* `<name of the disk image file>`
* `<size in blocks>` - 1 block = 1 kB

It is possible to mount the image to real system, as follows: `sudo mount fat32.img mnt/tst -t msdos -o loop=/dev/loop0`,
or more simplier: `sudo mount fat32.img mnt/tst -o loop`. The `/mnt/tst` should be replaced with real mount path.

The simplier variant automatically sets up the file system type and also automatically chooses the loop device. The `sudo`
command is needed only when another user than root uses the program.

You can create scripts that would be copy and delete a lot of files. This would cause disk fragmentation. When disk is
fragmented from more than 1%, the defrag utility will work. The defrag program can be executed in two ways. It can use
real disks, or image files. In the case of real disk, execute it as:

 `defrag /dev/sda5`, for example.

If the testing image is mounted, you can execute it in this way:

 `defrag /dev/loop0`, for example.

If no image is mounted, you can execute it with a file as parameter:

 `defrag fat32.img`, for example.


Release notes
--------------
 
* **v0.1b** (12.11.2006)
  - Fixed all relevant bugs, command line parameters added
* **v0.1a** (4.11.2006)
  - Simple defragmentation, manipulates with single cluster;
  - Not functioning checking of FAT table, working only with FAT32;
  - Algorithm as in Windows (pushes everything on the beginning).


