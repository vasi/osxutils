/*
    seticon - command line program to set icon of Mac OS X files
    Copyright (C) 2003-2005 Sveinbjorn Thordarson <sveinbt@hi.is>

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
	0.2 - * sysexits.h constants used as exit values
	0.1 - * Initial release
*/

#include <Carbon/Carbon.h>
#include <Foundation/Foundation.h>
#include "IconFamily.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sysexits.h>

#define		PROGRAM_STRING  	"seticon"
#define		VERSION_STRING		"0.2"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson"
#define		OPT_STRING			"vhd" 
//"vhdi"

static int UnixIsFolder (char *path);
static void PrintVersion (void);
static void PrintHelp (void);

int main (int argc, const char * argv[]) 
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

	int				rc, optch, sourceIsIcns = 0, sourceIsImage = 0;
	char			*src;
    static char		optstring[] = OPT_STRING;
	IconFamily		*icon;
	//NSImage		*image;
	NSString		*dstPath, *srcPath;

    while ( (optch = getopt(argc, (char * const *)argv, optstring)) != -1)
    {
        switch(optch)
        {
            case 'v':
                PrintVersion();
                exit(EX_OK);
                break;
            case 'h':
                PrintHelp();
                exit(EX_OK);
                break;
            case 'd':
				sourceIsIcns = 1;
				break;
			/*case 'i':
				sourceIsImage = 1;
				break;*/
            default: // '?'
                rc = 1;
                PrintHelp();
                exit(EX_USAGE);
        }
    }
	
	if (sourceIsIcns && sourceIsImage)
	{
		fprintf(stderr, "%s: Both -i and -d parameters specified.\nSource cannot both be icns and image", PROGRAM_STRING);
        PrintHelp();
        exit(EX_USAGE);
	}

	//check if a correct number of arguments was submitted
    if (argc < 3)
    {
        fprintf(stderr, "%s: Too few arguments.\n", PROGRAM_STRING);
        PrintHelp();
		exit(EX_USAGE);
    }
	
	(char *)src = (char *)argv[optind];

	//get the icon
	srcPath = [NSString stringWithCString: src];
	if (sourceIsIcns)
		icon = [IconFamily iconFamilyWithContentsOfFile: srcPath];
	/*else if (sourceIsImage)
	{
		image = [[NSImage alloc] initWithContentsOfFile: srcPath];
		icon = [IconFamily iconFamilyWithThumbnailsOfImage: image];
	//	[image release];
	}*/
	else
		icon = [IconFamily iconFamilyWithIconOfFile: srcPath];
	
	//all remaining arguments should be files
    for (; optind < argc; ++optind)
	{
		dstPath = [NSString stringWithCString: (char *)argv[optind]];
		if (UnixIsFolder((char *)argv[optind]))
			[icon setAsCustomIconForDirectory: dstPath];
		else
			[icon setAsCustomIconForFile: dstPath];
	}
	
    [pool release];
    return EX_OK;
}


/*//////////////////////////////////////
// Check if file in designated path is folder
/////////////////////////////////////*/
static int UnixIsFolder (char *path)
{
    struct stat filestat;
    short err;
    
    err = stat(path, &filestat);
    if (err == -1)
        return err;

    return (S_ISREG(filestat.st_mode) != 1);
}


#pragma mark -

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
    printf("usage: %s [-vhdi] [source] [file ...]\n", PROGRAM_STRING);
}

