/*
    geticon - command line program to get icon from Mac OS X files
    Copyright (C) 2004 Sveinbjorn Thordarson <sveinbt@hi.is>

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

	Version History
	
	0.2 - sysexits.h constants used as exit codes
	0.1 - geticon first released
	
*/

/*

	TO DO
	
	* Nothing I can think of.  Must be something, though.

*/

/////////////////// Includes //////////////////

#import <Foundation/Foundation.h>
#import "IconFamily.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sysexits.h>

/////////////////// Prototypes //////////////////

static int GenerateFileFromIcon (char *src, char *dst, int kind);
static int GetFileKindFromString (char *str);
static char* CutSuffix (char *name);
static char* GetFileNameFromPath (char *name);
static void PrintHelp (void);
static void PrintVersion (void);

/////////////////// Definitions //////////////////

#define		PROGRAM_STRING  	"geticon"
#define		VERSION_STRING		"0.2"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson"
#define		OPT_STRING			"vho:t:"

	//file kinds
	#define		kInvalidKindErr -1
	#define		kIcnsFileKind   0
	#define		kJpegFileKind   1
	#define		kBmpFileKind	2
	#define		kPngFileKind	3
	#define		kGifFileKind	4
	#define		kTiffFileKind   5

int main (int argc, const char * argv[]) 
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

	int				rc, optch, result, kind = kIcnsFileKind;
	char			*src = NULL, *dst = NULL;
	int				alloced = TRUE;
    static char		optstring[] = OPT_STRING;

    while ( (optch = getopt(argc, (char * const *)argv, optstring)) != -1)
    {
        switch(optch)
        {
            case 'v':
                PrintVersion();
                return EX_OK;
                break;
            case 'h':
                PrintHelp();
                return EX_OK;
                break;
            case 'o':
                dst = optarg;
				alloced = FALSE;
                break;
			case 't':
				kind = GetFileKindFromString(optarg);
				if (kind == kInvalidKindErr)
				{
					fprintf(stderr, "%s: %s: Invalid file kind\n", PROGRAM_STRING, optarg);
					return EX_USAGE;
				}
				break;
            default: // '?'
                rc = 1;
                PrintHelp();
                return EX_OK;
        }
    }

	(char *)src = (char *)argv[optind];

	//check if a correct number of arguments was submitted
    if (argc < 2 || src == NULL)
    {
        fprintf(stderr, "%s: Too few arguments.\n", PROGRAM_STRING);
        PrintHelp();
        return EX_USAGE;
    }
	
	
	//make destination icon file path current working directory with filename plus icns suffix
	if (dst == NULL)
	{
		dst = malloc(2048);
		strcpy(dst, src);
		strcpy(dst, (char *)GetFileNameFromPath(dst));
		dst = CutSuffix(dst);
	}
	
	result = GenerateFileFromIcon(src, dst, kind);
	
	if (alloced == TRUE)
		free(dst);
	
    [pool release];
    return result;
}

static int GetFileKindFromString (char *str)
{
	if (!strcmp(str, (char *)"jpeg"))
		return 1;
	if (!strcmp(str, (char *)"bmp"))
		return 2;
	if (!strcmp(str, (char *)"png"))
		return 3;
	if (!strcmp(str, (char *)"gif"))
		return 4;
	if (!strcmp(str, (char *)"tiff"))
		return 5;
	if (!strcmp(str, (char *)"icns"))
		return 0;
		
	
	return kInvalidKindErr;
}

static int GenerateFileFromIcon (char *src, char *dst, int kind)
{
	NSString	*srcStr = [NSString stringWithCString: src];
	NSString	*dstStr = [NSString stringWithCString: dst];
	NSData		*data;
	NSDictionary	*dict;
	
	//make sure source file we grab icon from exists
	if (![[NSFileManager defaultManager] fileExistsAtPath: srcStr])
	{
		fprintf(stderr, "%s: %s: No such file or directory\n", PROGRAM_STRING, src);
		return EX_NOINPUT;
	}
	
	IconFamily  *icon = [IconFamily iconFamilyWithIconOfFile: srcStr];
	
	if (kind == kIcnsFileKind)
	{
		if (![dstStr hasSuffix: @".icns"])
			dstStr = [dstStr stringByAppendingString:@".icns"];
		[icon writeToFile: dstStr];
	}
	else if (kind == kJpegFileKind)
	{
		if (![dstStr hasSuffix: @".jpg"])
			dstStr = [dstStr stringByAppendingString:@".jpg"];
		dict = [NSDictionary dictionaryWithObject:[NSNumber numberWithFloat:1.0] forKey:NSImageCompressionFactor];
		data = [[icon bitmapImageRepWithAlphaForIconFamilyElement: kThumbnail32BitData] representationUsingType:NSJPEGFileType properties:dict];
		[data writeToFile: dstStr atomically:YES];
	}
	else if (kind == kBmpFileKind)
	{
		if (![dstStr hasSuffix: @".bmp"])
			dstStr = [dstStr stringByAppendingString:@".bmp"];
		dict = [NSDictionary dictionaryWithObject:[NSNumber numberWithFloat:1.0] forKey:NSImageCompressionFactor];
		data = [[icon bitmapImageRepWithAlphaForIconFamilyElement: kThumbnail32BitData] representationUsingType:NSBMPFileType properties:dict];
		[data writeToFile: dstStr atomically:YES];
	}
	else if (kind == kPngFileKind)
	{
		if (![dstStr hasSuffix: @".png"])
			dstStr = [dstStr stringByAppendingString:@".png"];
		dict = [NSDictionary dictionaryWithObject:[NSNumber numberWithFloat:1.0] forKey:NSImageCompressionFactor];
		data = [[icon bitmapImageRepWithAlphaForIconFamilyElement: kThumbnail32BitData] representationUsingType:NSPNGFileType properties:dict];
		[data writeToFile: dstStr atomically:YES];
	}
	else if (kind == kGifFileKind)
	{
		if (![dstStr hasSuffix: @".gif"])
			dstStr = [dstStr stringByAppendingString:@".gif"];
		dict = [NSDictionary dictionaryWithObject:[NSNumber numberWithFloat:1.0] forKey:NSImageCompressionFactor];
		data = [[icon bitmapImageRepWithAlphaForIconFamilyElement: kThumbnail32BitData] representationUsingType:NSGIFFileType properties:dict];
		[data writeToFile: dstStr atomically:YES];
	}
	else if (kind == kTiffFileKind)
	{
		if (![dstStr hasSuffix: @".tiff"])
			dstStr = [dstStr stringByAppendingString:@".tiff"];
		dict = [NSDictionary dictionaryWithObject:[NSNumber numberWithFloat:1.0] forKey:NSImageCompressionFactor];
		data = [[icon bitmapImageRepWithAlphaForIconFamilyElement: kThumbnail32BitData] representationUsingType:NSTIFFFileType properties:dict];
		[data writeToFile: dstStr atomically:YES];
	}
	
	//see if file was created
	if (![[NSFileManager defaultManager] fileExistsAtPath: dstStr])
	{
		fprintf(stderr, "%s: %s: File could not be created\n", PROGRAM_STRING, dst);
		return EX_CANTCREAT;
	}
	
	return EX_OK;
}

////////////////////////////////////////
// Cuts suffix from a file name
///////////////////////////////////////
static char* CutSuffix (char *name)
{
    short	i, len, suffixMaxLength = 11;
    
    len = strlen(name);
    
    for (i = 1; i < suffixMaxLength+2; i++)
    {
        if (name[len-i] == '.')
        {
            name[len-i] = NULL;
            return name;
        }
    }
    return name;
}





static char* GetFileNameFromPath (char *name)
{
    short	i, len;
    
    len = strlen(name);
    
    for (i = len; i > 0; i--)
    {
        if (name[i] == '/')
        {
            return((char *)&name[i+1]);
        }
    }
    return name;
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
    printf("usage: %s [-vh] [-t [icns|png|gif|tiff|jpeg]] [-o outputfile] [file]\n", PROGRAM_STRING);
}
