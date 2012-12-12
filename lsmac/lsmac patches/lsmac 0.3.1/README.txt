README.txt file for lsmac 0.3 created 06/04/2003.

ABOUT

"lsmac" is a command line utility for MacOS X created to display directory contents in a similar way to the unix "ls" program, but with a more Mac-centric approach.  "lsmac" will list the files and folders within a given directory with the associated heritage Mac file data such as File and Creator Types and Finder flags.  "lsmac" can also list file sizes like the ls program, but displays the size of all the file's forks, not just the data fork, thus giving correct size for files containing heritage MacOS resource forks.  For information on how to use "lsmac", type "man lsmac" in the command line once you've installed it.

lsmac is open-source software and is distributed under the terms of the GNU General Public License, of whom you will find a copy in this folder.

INSTALL

To install the precompiled 'lsmac' executable, open the Terminal, change to the folder containing this readme file and type in the following command:

sudo sh install.sh

To uninstall type the following:

sudo sh uninstall.sh

The program is installed in /usr/bin/.  You can change this by modifying the shell scripts.  If you want to compile 'lsmac' yourself, then open the Terminal, change to the Source folder and type the following:

make
sudo make install

To uninstall, you instead type:

sudo make uninstall

In this case, lsmac is installed into /usr/local/bin.

I hope 'lsmac' proves useful.  If you have any comments, criticism or suggestions for features then please don't hesistate to send me email.

Sincerely,

Sveinbjorn Thordarson
sveinbt@hi.is
