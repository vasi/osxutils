#!/bin/sh


cp lsmac /usr/bin/lsmac
cp lsmac.1 /usr/share/man/man1/lsmac.1

if [ -e /usr/bin/lsmac ]; then
	echo "Install completed."
else
	echo "Install failed.  Try again with super user privileges.";
fi

