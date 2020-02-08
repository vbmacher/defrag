# DeFrag

The program performs defragmentation of FAT32 filesystem. It was created as school assignment for the subject
"Operating systems" taught at Technical University of Košice, in 2006.  

## License

This project is released under GNU GPL v2 license.

## Building

In order to build the program, the following tools are needed to be installed: `gcc`, `make`, `xgettext`, and `sed`.

NOTE: The `xgettext` program is used for automatic retrieving of strings that will be localized. So the program is multilingual.
      Additional translation and creation of binary localization file is needed to perform manually. Currently,
      there are supported just English and Slovak languages.

There is prepared custom `Makefile`. In order to compile the source code, type: `make`.
Executable file is called `defrag`.

## Using

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

## Few words about the algorithm

### Fragmentation 

Files stored on a filesystem consist of blocks of data, usually fixed in size. Fragmentation of a file, in FAT32 or
(other similar filesystems) means that those blocks are unordered and not continuous. Fragmentation, depending how
big it is, can be a root cause for slow file access.
 
Depending on a use-case, if the file is accessed continuously at once, it is much better if data blocks are stored in
ordered way, so the mechanical head in the harddisk (if we don't have SSD) can do as little movements as possible. There
are two root causes of the fragmentation - unordered placement of blocks, and non-continuous placement of blocks.
 
The worst case for "broken" ordering is when data blocks are stored in reversed order. If we want to read the file from
start to the end, disk cylinder must do one full rotation to access single data block.

The worst case for non-continuous blocks is if each block is stored on different track, but in a way that the head must
move in both directions. If tracks from the cylinder center are marked as [0,1,2,3,4], then the worst case would be if
the blocks are stored on tracks 0,4,0,4, etc.  

Implementation of some filesystems (e.g. [ext2](http://en.wikipedia.org/wiki/Ext2#ext2_data_structures) or [hpfs](http://en.wikipedia.org/wiki/HPFS) )
are designed in a way that the fragmentation would be minimal. One technique is to write files in sparse way, so between
files should be some significant "distance" (free space). In that case if a file is deleted, more continuous space will
be available for new files than before, which the risk of creating fragmentation after writing the new file is lowered.
Ofcourse, if there is not much free space, the fragmentation will strike back.

FAT filesystem in Windows is not implemented this way, so fragmentation can occur more often. Therefore user must often
reach to a defragmentation software, such as this one, which fixes the blocks ordering and their continuity. 

### Defragmentation algorithm

Good disk analysis is base for good defragmentation. It is a first step of the process. The main idea of the analysis
is to recursively traverse whole directory structure (starting at the root node) and remember (for the simplicity) the
blocks (in FAT - clusters) locations (linked list) of each file or directory found.

Second step is to sort the clusters in memory, taking into account also things like bad clusters (which are unmovable).
The sorting is very simple, e.g. the trick with lowering further fragmentation by placing free space between files is not
implemented. Files and directories are "pushed" one next to other, starting from the first available disk position.

The last step is to perform physical replacement of the clusters to apply the order created in the previous step. The
replacement works in a cycle. Each iteration processes a pair of clusters - the next file cluster in order, and the
cluster which stays in it's way - they are physically switched.  

### Further optimizations

The sorting algorithm can be improved in a way to prevent further fragmentation (be more future-proof), or to be faster -
e.g. just make things in order, but keep blocks stored in non-continuous positions.  

The replacement algorithm can be improved as well. For replacement, several continuous blocks can be replaced at once
(reading from multiple locations, writing at one location) so the reading of one cluster wouldn't have to return to
writing position immediately, but just after reading several clusters. I call it "school bus" algorithm - a school bus
collects children at various places but dump them at once near the school. 


## Release notes
 
* **v0.1b** (12.11.2006)
  - Fixed all relevant bugs, command line parameters added
* **v0.1a** (4.11.2006)
  - Simple defragmentation, manipulates with single cluster;
  - Not functioning checking of FAT table, working only with FAT32;
  - Algorithm as in Windows (pushes everything on the beginning).


