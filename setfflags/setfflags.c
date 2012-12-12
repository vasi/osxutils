/*
    setfflags - program to set Mac Finder flags of files
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
    
    0.1 - First release of setfflags
    0.2 - Updated to support folders
	0.3 - Code cleaned, sysexits.h constants used for exit values, errors go to stderr

*/

/*
    Command line options

    v - version
    h - help - usage
    m - silent mode
    p - print flags
    
    // flag options

        c - has custom icon
        s - stationery
        l - name locked
        b - has bundle
        i - is invisible
        a - is alias
    
*/


#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <Carbon/Carbon.h>
#include <string.h>
#include <sysexits.h>

/////////////////// Definitions //////////////////

#define PROGRAM_STRING  "setfflags"
#define VERSION_STRING  "0.3"
#define AUTHOR_STRING   "Sveinbjorn Thordarson"

#define OPT_STRING      "mpvhc:s:l:b:i:a:"

/////////////////// Prototypes //////////////////

static int SetFlags(char *fileStr);
static int PrintFlags(char *fileStr);
static short UnixIsFolder(char *path);
static Boolean GetBooleanParameter(char *str);
static void PrintVersion(void);
static void PrintHelp(void);

/////////////////// Globals //////////////////

enum 
{
    kDoCustomIcon = 0,
    kDoStationery,
    kDoNameLocked,
    kDoHasBundle,
    kDoIsInvisible,
    kDoIsAlias,
    kLastFinderFlag
};

enum 
{
    kDoCustomIconBit = 1 << kDoCustomIcon,
    kDoStationeryBit = 1 << kDoStationery,
    kDoNameLockedBit = 1 << kDoNameLocked,
    kDoHasBundleBit = 1 << kDoHasBundle,
    kDoIsInvisibleBit = 1 << kDoIsInvisible,
    kDoIsAliasBit = 1 << kDoIsAlias
};

static const UInt16 kFinderFlags[] = 
{
    kHasCustomIcon,
    kIsStationery,
    kNameLocked,
    kHasBundle,
    kIsInvisible,
    kIsAlias
};

static const UInt16 kFolderOnlyFlags[] = 
{
    kHasCustomIcon,
    0,
    kNameLocked,
    0,
    kIsInvisible,
    0
};

static const char *kFinderStrings[] = 
{
    "kHasCustomIcon",
    "kIsStationery",
    "kNameLocked",
    "kHasBundle",
    "kIsInvisible",
    "kIsAlias"
};

static short   silentMode;
static short   printFlags;
static UInt16  sCustomFlags;
static UInt16  sCustomMask;

////////////////////////////////////////////
// main program function
////////////////////////////////////////////
int main (int argc, const char * argv[])
{
    int            err = EX_OK;
    int            optch;
    static char    optstring[] = OPT_STRING;

    silentMode = false;
    printFlags = false;
    sCustomFlags = 0;
    sCustomMask = 0;

    while ( (optch = getopt(argc, (char * const *)argv, optstring)) != -1) 
	{
        switch (optch) 
		{
            case 'm':
                silentMode = true;
                break;
            case 'v':
                PrintVersion();
                return 0;
                break;
            case 'h':
                PrintHelp();
                return 0;
                break;
            case 'c':
                if (GetBooleanParameter(optarg))
                    sCustomFlags |= kDoCustomIconBit;
                sCustomMask |= kDoCustomIconBit;
                break;
            case 's':
                if (GetBooleanParameter(optarg))
                    sCustomFlags |= kDoStationeryBit;
                sCustomMask |= kDoStationeryBit;
                break;
            case 'l':
                if (GetBooleanParameter(optarg))
                    sCustomFlags |= kDoNameLockedBit;
                sCustomMask |= kDoNameLockedBit;
                break;
            case 'b':
                if (GetBooleanParameter(optarg))
                    sCustomFlags |= kDoHasBundleBit;
                sCustomMask |= kDoHasBundleBit;
                break;
            case 'i':
                if (GetBooleanParameter(optarg))
                    sCustomFlags |= kDoIsInvisibleBit;
                sCustomMask |= kDoIsInvisibleBit;
                break;
            case 'a':
                if (GetBooleanParameter(optarg))
                    sCustomFlags |= kDoIsAliasBit;
                sCustomMask |= kDoIsAliasBit;
                break;
            case 'p':
                printFlags = true;
                break;
            default: // '?'
                PrintHelp();
                return 0;
        }
    }
    
    // if there are no types specifed and we're not printing the flags, print help and exit
    if (!sCustomMask && !printFlags) 
	{
        PrintHelp();
        return EX_USAGE;
    }

    if (sCustomMask && printFlags) 
	{
        fprintf(stderr, "Incompatible options.\n");
        PrintHelp();
        return EX_USAGE;
    }

    // all remaining arguments should be files
    for (; optind < argc && !err; ++optind) 
	{
        if (printFlags)
            err = PrintFlags((char *)argv[optind]);
        else
            err = SetFlags((char *)argv[optind]);
    }
    
    return err;
}



#pragma mark -

////////////////////////////////////////////
// Use Carbon functions to actually set the
// File and Creator types
////////////////////////////////////////////

static int SetFlags (char *fileStr)
{
    int              i;
    const UInt16     *pFlags;
    OSErr            err;
    FSRef            fileRef;
    FSCatalogInfo    catalogInfo;
    FInfo            *fInfo;
    UInt16           finderFlags;

    // see if the file in question exists and we can write it
    if (access(fileStr, R_OK|W_OK|F_OK) == -1) 
	{
        if (!silentMode)
            perror(fileStr);
        return EX_NOPERM;
    }

    // check if it's a folder
    i = UnixIsFolder(fileStr);
    if (i == -1) 
	{
        if (!silentMode)
            fprintf(stderr, "UnixIsFolder: Error %d for file %s\n", errno, fileStr);
        return EX_NOINPUT;
    }

    // enable folder-only or complete flags
    pFlags = (i == 1) ? kFolderOnlyFlags : kFinderFlags;

    // get file reference from path
    if ((err = FSPathMakeRef(fileStr, &fileRef, NULL)) != noErr) 
	{
        if (!silentMode)
            fprintf(stderr, "FSPathMakeRef: Error %d for file %s\n", err, fileStr);
        return EX_NOINPUT;
    }

    // retrieve finder info from file ref
    if ((err = FSGetCatalogInfo(&fileRef, kFSCatInfoFinderInfo, &catalogInfo, NULL, NULL, NULL)) != noErr) 
	{
        if (!silentMode)
            fprintf(stderr, "FSGetCatalogInfo(): Error %d getting Finder info for %s\n", err, fileStr);
        return EX_NOINPUT;
    }

    fInfo = (FInfo *)&catalogInfo.finderInfo;
    finderFlags = fInfo->fdFlags;

    // Modify the finder flags structure
    for (i = 0; i < kLastFinderFlag; ++i) 
	{
        if ((sCustomMask & (1 << i))) 
		{
            if (pFlags[i]) 
			{
                if ((sCustomFlags & (1 << i)))
                    finderFlags |= pFlags[i];
                else
                    finderFlags &= ~pFlags[i];
            } 
			else 
			{
                if (!silentMode)
                    fprintf(stderr, "Unsupported flag %s for %s\n", kFinderStrings[i], fileStr);
                return EX_USAGE;
            }
        }
    }

    // If the finder flags we're supposed to set
    // aren't the same as the ones already in place, we set them.
    if (fInfo->fdFlags != finderFlags)
	{
        fInfo->fdFlags = finderFlags;
        if ((err = FSSetCatalogInfo(&fileRef, kFSCatInfoFinderInfo, &catalogInfo)) != noErr) 
		{
            if (!silentMode)
                fprintf(stderr, "FSSetCatalogInfo(): Error %d setting Finder info for %s\n", err, fileStr);
            return EX_IOERR;
        }
    }
    return EX_OK;
}

static int PrintFlags (char *fileStr)
{
    int              i;
    const UInt16     *pFlags;
    OSErr            err;
    FSRef            fileRef;
    FSCatalogInfo    catalogInfo;
    FInfo            *fInfo;

    //see if the file in question exists and we can read it
    if (access(fileStr, R_OK|F_OK) == -1) 
	{
        if (!silentMode)
            perror(fileStr);
        return EX_NOPERM;
    }

    //check if it's a folder
    i = UnixIsFolder(fileStr);
    if (i == -1) 
	{
        if (!silentMode)
            fprintf(stderr, "UnixIsFolder: Error %d for file %s\n", errno, fileStr);
        return EX_IOERR;
    }

    //enable folder-only or complete flags
    pFlags = (i == 1) ? kFolderOnlyFlags : kFinderFlags;

    //get file reference from path
    err = FSPathMakeRef(fileStr, &fileRef, NULL);
    if (err != noErr) 
	{
        if (!silentMode)
            fprintf(stderr, "FSPathMakeRef: Error %d for file %s\n", err, fileStr);
        return EX_IOERR;
    }
    
    //retrieve finder info from file ref
    err = FSGetCatalogInfo(&fileRef, kFSCatInfoFinderInfo, &catalogInfo, NULL, NULL, NULL);
    if (err != noErr) 
	{
        if (!silentMode)
            fprintf(stderr, "FSGetCatalogInfo(): Error %d getting Finder info for %s\n", err, fileStr);
        return EX_IOERR;
    }
    fInfo = (FInfo *)&catalogInfo.finderInfo;

    printf("File: %s\n", fileStr);
    printf("Flags:\n");

    // Display the finder flags structure
    for (i = 0; i < kLastFinderFlag; ++i)
        if ( pFlags[i] )
            printf("%22s - %s", kFinderStrings[i], (fInfo->fdFlags & pFlags[i]) != 0 ? "On\n" : "Off\n");
        
    printf("\n");
    return EX_OK;
}


#pragma mark -

////////////////////////////////////////
// Check if file in designated path is folder
///////////////////////////////////////
static short UnixIsFolder (char *path)
{
    struct stat  filestat;
    short        err;
    
    err = stat(path, &filestat);
    if (err == -1)
        return err;

    if(S_ISREG(filestat.st_mode) != 1)
        return true;
    else 
        return false;
}

#pragma mark -

////////////////////////////////////////////
// Get boolean value from string argument
// Can be specified as 0, 1, false or true
////////////////////////////////////////////

static Boolean GetBooleanParameter (char *str)
{
    char     numStr[2];
    short    num;
    
    if (!strcmp(str, "true"))
        return true;
    if (!strcmp(str, "false"))
        return false;
    
    if (strlen(str) != 1)
    {
        fprintf(stderr, "%s: Invalid parameter for option.\n", str);
        exit(EX_USAGE);
    }

    strcpy(numStr, str);
    num = atoi(numStr);
    
	if (num != 0 && num != 1)
    {
        fprintf(stderr, "%d: Invalid parameter for option.\n", num);
        exit(EX_USAGE);
    }
    
    return num;
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
    printf("usage: %s [-mvh] [-c bool] [-s bool] [-i bool] [-l bool] [-b bool] [-a bool] [file ...]\n", PROGRAM_STRING);
    printf("   or: %s [-p] [file ...]\n", PROGRAM_STRING);
}
