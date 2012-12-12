/*
    setfctypes - program to set heritage Mac file/creator types of files 
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


/*  CHANGES
    
	0.2 - Exit codes use sysexits.h constants, errors go to stderr
    0.1 - First release of setfctypes

*/

/*  TODO
        
        * Recursively scan through folder hierarchies option (can currently be done by combining with 'find')
        * Hmmm...suggestions are welcome
*/


/*
    Command line options

    v - version
    h - help - usage
    c - set creator type
    f - set file type
    s - silent mode
    
*/


#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <Carbon/Carbon.h>
#include <string.h>
#include <sysexits.h>

/////////////////// Definitions //////////////////

#define		PROGRAM_STRING  	"setfctypes"
#define		VERSION_STRING		"0.2"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson"

#define		OPT_STRING		"svhf:c:"

/////////////////// Prototypes //////////////////

static void SetTypes (char *fileStr, OSType fileType, OSType creatorType);
static OSType GetOSTypeFromString (char *str);
static OSType CStrToType(char *s);
//Boolean IsFolder (FSRef *fileRef);
static short UnixIsFolder (char *path);
static void PrintVersion (void);
static void PrintHelp (void);

/////////////////// Globals //////////////////

short		silentMode = false;


////////////////////////////////////////////
// main program function
////////////////////////////////////////////
int main (int argc, const char * argv[])
{
    int			rc;
    int			optch;
    static char	optstring[] = OPT_STRING;
    
    OSType		fileType = (OSType)NULL;
    OSType		creatorType = (OSType)NULL;

    while ( (optch = getopt(argc, (char * const *)argv, optstring)) != -1)
    {
        switch(optch)
        {
            case 's':
                silentMode = true;
                break;
            case 'v':
                PrintVersion();
                return EX_OK;
                break;
            case 'h':
                PrintHelp();
                return EX_OK;
                break;
            case 'f':
                fileType = GetOSTypeFromString(optarg);
                break;
            case 'c':
                creatorType = GetOSTypeFromString(optarg);
                break;
            default: // '?'
                rc = 1;
                PrintHelp();
                return EX_USAGE;
        }
    }
    
    //if there are no types specifed, print help and exit
    if (fileType == (OSType)NULL && creatorType == (OSType)NULL)
    {
        PrintHelp();
        return EX_USAGE;
    }

    //all remaining arguments should be files
    for (; optind < argc; ++optind)
        SetTypes((char *)argv[optind], fileType, creatorType);

    return EX_OK;
}


#pragma mark -

////////////////////////////////////////////
// Use Carbon functions to actually set the
// File and Creator types
////////////////////////////////////////////

static void SetTypes (char *fileStr, OSType fileType, OSType creatorType)
{
    OSErr	err = noErr;
    FSRef	fileRef;
    FSSpec	fileSpec;
    FInfo 	finderInfo;
    short	c = 0;

    //see if the file in question exists and we can write it
    if (access(fileStr, R_OK|W_OK|F_OK) == -1)
    {
        if (!silentMode)
            perror(fileStr);
        return;
    }
    
    //get file reference from path
    err = FSPathMakeRef(fileStr, &fileRef, NULL);
    if (err != noErr)
    {
        if (!silentMode)
            fprintf(stderr, "FSPathMakeRef: Error %d for file %s\n", err, fileStr);
        return;
    }
    
    //check if it's a folder
    c = UnixIsFolder(fileStr);
    
    if (c == -1)//an error occurred in stat
    {
        if (!silentMode)
            perror(fileStr);
        return;
    }
    else if (c == 1)//it is a folder
    {
        if (!silentMode)
            fprintf(stderr, "%s is a folder\n", fileStr);
        return;
    }
    
    //retrieve filespec from file ref
    err = FSGetCatalogInfo (&fileRef, NULL, NULL, NULL, &fileSpec, NULL);
    if (err != noErr)
    {
        if (!silentMode)
            fprintf(stderr, "FSGetCatalogInfo(): Error %d getting file spec for %s", err, fileStr);
        return;
    }
    
    //get the finder info   
    err = FSpGetFInfo (&fileSpec, &finderInfo);
    if (err != noErr)
    {
        if (!silentMode)
            fprintf(stderr, "FSpGetFInfo(): Error %d getting Finder info for %s", err, fileStr);
        return;
    }
    
    c = false;
    
    //now, change the creator/file types in FInfo structure as appropriate
    if (fileType != (OSType)NULL && finderInfo.fdType != fileType)
    {
        finderInfo.fdType = fileType;
        c = true;
    }
    if (creatorType != (OSType)NULL && finderInfo.fdCreator != (OSType)creatorType)
    {
        finderInfo.fdCreator = creatorType;
        c = true;
    }
    
    //if the file and creator types we're setting are the same as the file's
    //then there's no need to set them.  A little optimization never hurt... :)
    if (!c)
        return;
    
    err = FSpSetFInfo (&fileSpec, &finderInfo);
    if (err != noErr)
    {
        if (!silentMode)
            fprintf(stderr, "FSpSetFInfo(): Error %d setting Finder info for %s", err, fileStr);
        return;
    }
}

#pragma mark -

////////////////////////////////////////////
// Create an Apple OSType from a C string
////////////////////////////////////////////

static OSType GetOSTypeFromString (char *str)
{
    OSType	type;
    
    if (strlen(str) != 4)
    {
        fprintf(stderr, "Illegal parameter: %s\nYou must specify a string of exactly 4 characters\n", str);
        exit(EX_DATAERR);
    }
    
    type = CStrToType(str);
    
    return(type);
}

////////////////////////////////////////
// Transform  a C string into OSType
//
// We could also just do:  typeCode = (OSType)*s;
// However, that would only work on big-endian machines
// We wouldn't want that now, would we?
//
// Thanks go to Ed Watkeys for this one
///////////////////////////////////////

static OSType CStrToType(char *s)
{
   OSType   typeCode = (OSType)NULL;

   typeCode = ((s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3]); 

   return typeCode;
}

#pragma mark -

////////////////////////////////////////
// Check if file in designated path is folder
///////////////////////////////////////
static short UnixIsFolder (char *path)
{
    struct stat filestat;
    short err;
    
    err = stat(path, &filestat);
    if (err == -1)
        return err;

    if(S_ISREG(filestat.st_mode) != 1)
        return true;
    else 
        return false;
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
    printf("usage: %s [-vhs] [-f filetype] [-c creator] [file ...]\n", PROGRAM_STRING);
}
