#!/bin/sh


rm -f lsmac /usr/bin/lsmac
rm -f lsmac.1 /usr/share/man/man1/lsmac.1

if [ -e /usr/bin/lsmac ]; then
	echo "Uninstall failed.  Try again with super user privileges.";
else
	echo "Uninstall completed."
fi

