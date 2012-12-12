/*
    wsupdate - command line program which notifies the Mac OS X Workspace Manager of new files
    Copyright (C) 2005 Sveinbjorn Thordarson <sveinbt@hi.is>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/*
	In versions of Mac OS X prior to 10.4 "Tiger", the Finder file manager
	updated the files in windows via polling, i.e. by checking at certain
	intervals whether new files had been created.  This could result in a 
	considerable delay between the creation of a file by a command line
	program and its subsequent display in Finder windows.
	
	wsupdate was created to circument this problem.  A command line script
	can call this program and pass the paths of newly created files to send
	a message to the workspace manager notifying of their existence.  This
	will cause the Finder to display them instantly.  Example:
	
	wsupdate ~/Desktop/MyNewFile.txt
	
	wsupdate uses the same API call to notify the workspace manager as regular
	Mac OS X applications.  If no files are passed as arguments, wsupdate
	just sends a general "file system changed" notification, which should update
	all Finder windows, regardless of contents.
*/

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <sysexits.h>

#define		PROGRAM_STRING  	"wsupdate"
#define		VERSION_STRING		"0.2"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson"
#define		OPT_STRING			"vh" 

/*
		Version History
		
		0.2 - * Updated to use BSD sysexits
			  * Now complains if a path does not exist
			  * Functionality moved to UpdateWorkspaceForFile function
		
		0.1 - * Initial Release
*/

static void UpdateWorkspaceForFile (char *path);
static void PrintVersion (void);
static void PrintHelp (void);

int main (int argc, const char * argv[]) 
{
	int					rc, optch;
	static char			optstring[] = OPT_STRING;
    NSAutoreleasePool	*pool = [[NSAutoreleasePool alloc] init];
	

    while ( (optch = getopt(argc, (char * const *)argv, optstring)) != -1)
    {
        switch(optch)
        {
            case 'v':
                PrintVersion();
                return(EX_OK);
                break;
            case 'h':
                PrintHelp();
                return(EX_OK);
                break;
            default: // '?'
                rc = 1;
                PrintHelp();
                return(EX_OK);
        }
    }

	// if no file or folder is submitted as argument,
	// we just send a general file system changed notification
	if (argc == 0)
	{
		[[NSWorkspace sharedWorkspace] noteFileSystemChanged];
	}
	else
	{
		// otherwise, we send change notification for all the specified files/folders
		for (; optind < argc; ++optind)
		{
			UpdateWorkspaceForFile((char *)argv[optind]);
		}
	}

    [pool release];
    return(EX_OK);
}

#pragma mark -

//////////////////////////////////////////////
// Update workspace for a given c string path
// and give error if no file exists at path
//////////////////////////////////////////////

static void UpdateWorkspaceForFile (char *path)
{
	NSString *pathStr = [NSString stringWithCString: path];
	
	if ([[NSFileManager defaultManager] fileExistsAtPath: pathStr])
		[[NSWorkspace sharedWorkspace] noteFileSystemChanged: pathStr];
	else
		fprintf(stderr, "No such file or directory: %s\n", path);
}

////////////////////////////////////////
// Print version and author to stdout
///////////////////////////////////////

static void PrintVersion (void)
{
    printf("%s version %s by %s\n", PROGRAM_STRING, VERSION_STRING, AUTHOR_STRING);
}

////////////////////////////////////////
// Print help string to stdout
///////////////////////////////////////

static void PrintHelp (void)
{
    printf("usage: %s [-vh] [directory ...]\n", PROGRAM_STRING);
}


