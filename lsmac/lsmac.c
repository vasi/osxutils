/*
    lsmac - ls-like utility for viewing Mac OS file meta-data
    Copyright (C) 2003-2005 	Sveinbjorn Thordarson <sveinbt@hi.is>
								Ingmar J. Stein <stein@xtramind.com>
								Jean-Luc Dubois
								
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
// I wrote this program because I found the Apple-supplied command line tools with the 
//  Developer Tools to be rather poor and needed a convenient way to view classic Mac file meta-data.
//  I'd appreciate being notified of any changes/improvements made to the lsmac source code 
//  so I can add them to my own releases.  Thanks and enjoy --  Sveinbjorn Thordarson <sveinbt@.hi.is>
*/

/*  CHANGES

	0.6	-	* Now lists symlinks without error, thanks to Jean-Luc Dubois
			* All errors go to stderr
			* Exit values are constants from sysexits.h

    0.5 -   * Added support for labels now that they're back in the Mac OS as of version 10.3 "Panther"
                    * Label name is prepended to each output line with the -L option
            * Finder flags for folders are now also retrieved and displayed
            * Memory usage and performance improved ever so slightly by hardcoding max path length
            * Removed deprecated code, slightly better commenting and help output
    

    0.4 -   * made internal methods static (reduced file size by more than 50%)
            * removed dynamic memory allocation and other performance enhancements
            * Defined KB, MB and GB as 2^10, 2^20 and 2^30 bytes, respectively
            * List multiple directories given as command line arguments
            * Mac OS Finder Alias files now listed like symlinks with standard ls: *alias* --> *source*

    0.3 -   * lsmac can now be told whether to list the size of resource fork, data fork or both.  Both is default
            * -Q option: names/paths printed within quotation marks
            * lsmac now displays file size in human readable format by default
            * fixed error that occurred when listing root directory
            * -F option: only folders are listed
            * Num. of files within folders is now listed by default
            * Some minor optimizations (f.e. stat used for determining directories, free & malloc instead of NewPtr & DisposePtr, etc.)

    0.2 - Tons of stuff changed/improved
    
            * lsmac now accepts command line options via getopt
            * can display file size in bytes or human readable format
                * The size displayed is the size of ALL the file's forks, not just the data fork!
                * Option to display the physical size of file instead of logical
            * can display full pathname of files instead of just the names
            * can be set to omit directories in listing directory contents
            * lsmac now has a man page lsmac(1)
    
    0.1 - First release of lsmac

*/


/*  TODO
    
        * Incorporate Ingmar J. Stein's improvements to lsmac, which should fix the following problems
        
            * Bug - errors may occur when listing folders containing more than 9999 files.
            * Bug - Does not display size correctly for files larger than 4GB or so. 64bit int problem.
            * Bug - When a directory parameter ends with a "/", the pathnames reported by the -p option have two slashes at the end
        
        * Implement calculation of folder sizes - recursively scan through hierarchy and add up total size
        * There is other Mac file meta-data not available via FSpGetFInfo() which should also be listed
        * -x option: list file suffixes in a seperate column, kind of like the DOS dir command
        * List total size/number of all files listed on top, akin to ls
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <Carbon/Carbon.h>
#include <sysexits.h>
#include <string.h>

/*///////Prototypes///////////////////*/

static void PrintVersion (void);
static void PrintHelp (void);

static void ListDirectoryContents (char *arg);
static void ListItem (char *path, char *name);
static void ListFile (char *name, char *path, FSRef fileRef);
static void ListFolder (char *name, char *path, FSRef fileRef);

static char* GetNumFilesString (long numFiles);
static long GetNumFilesInFolder (FSRef *fileRef);

static short GetForkParameterFromString (char *str);

// static char* GetSizeString (long size);
static char* GetSizeString (UInt64 size);   // up to 99 TBytes
// static char* GetHumanSizeString (long size);
static char* GetHumanSizeString (UInt64 size);

static OSErr GetForkSizes (const FSRef *fileRef,  UInt64 *totalLogicalForkSize, UInt64 *totalPhysicalForkSize, short fork);

static void OSTypeToStr(OSType aType, char *aStr);
static int UnixIsFolder (char *path);
static void HFSUniPStrToCString (HFSUniStr255 *uniStr, char *cstr);

static char* GetPathOfAliasSource (char *path);
static OSStatus FSMakePath(FSRef fileRef, UInt8 *path, UInt32 maxPathSize);
static OSErr FSpGetDInfo(const FSSpec* fileSpec, DInfo *dInfo);
static short GetLabelNumber (SInt16 flags);

static OSErr MyFSPathMakeRef( const unsigned char *path, FSRef *ref );  // path to the link itself
static OSErr ConvertCStringToHFSUniStr(const char* cStr, HFSUniStr255 *uniStr);

static UInt64 totalFolderSize;     // total size of files in folder

/*///////Definitions///////////////////*/

#define		PROGRAM_STRING  	"lsmac"
#define		VERSION_STRING		"0.5"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson <sveinbt@hi.is>"

/* Text for /usr/bin/what */
/*@unused@*/ static const char rcsid[] = "@(#)" PROGRAM_STRING " " VERSION_STRING
    " $Id: lsmac.c,v 1.5 2004/12/19 22:59:06 carstenklapp Exp $";

#define         USAGE_STRING            "lsmac [-LvhFsboaplQ] [-f fork] directory ..."

#define		MAX_PATH_LENGTH		1024
#define		MAX_FILENAME_LENGTH	256

#define		OPT_STRING		"Lvhf:FsboaplQ"

#define		DISPLAY_FORK_BOTH	0
#define		DISPLAY_FORK_DATA	1
#define		DISPLAY_FORK_RSRC	2

#define		BOTH_FORK_PARAM_NAME	"both"
#define		DATA_FORK_PARAM_NAME	"data"
#define		RSRC_FORK_PARAM_NAME	"rsrc"

#define		IS_SYMLINK			3			  // to deal with symlinks
#define		VOL_NOT_FOUND		-35       // to suppress error with /.vol & /dev
#define kCouldNotCreateCFString	4
#define kCouldNotGetStringData	5


/*///////Globals///////////////////*/

static int		omitFolders = false;
static int		displayAll = false;
static int		printFullPath = false;
static int		useBytesForSize = false;
static int		physicalSize = false;
static int		forkToDisplay = DISPLAY_FORK_BOTH;
static int		useQuotes = false;
static int      printLabelName = false;
static int		foldersOnly = false;

static char             labelNames[8][8] = { "None   ", "Red  ", "Orange ", "Yellow ", "Green  ", "Blue   ", "Purple ", "Gray   " };


/*
    Command line options

    v - version
    h - help
    B - display size as 10^X
    b - display size in bytes
    s - display size as 2^z
    o - omit folders - will not omit folder aliases
    a - display all (including hidden . files)
    p - print full file path
    l - when printing size, print physical size, not logical size
    L - print label name
    
    [-f fork] - select which fork to print size of
    
    c - calculate folder sizes 				** NOT IMPLEMENTED YET **
    i - calculate number of files within folders 	** NOT IMPLEMENTED YET **

*/

/*//////////////////////////////////////
// Main program function
/////////////////////////////////////*/
int main (int argc, char *argv[]) 
{
    int			i;
    int			rc;
    int			optch;
    static char		optstring[] = OPT_STRING;
    char                buf[MAX_PATH_LENGTH];
    char                *cwd;

    while ( (optch = getopt(argc, argv, optstring)) != -1)
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
            case 'f':
                forkToDisplay = GetForkParameterFromString(optarg);
                break;
            case 'F':
                foldersOnly = true;
                break;
            case 'b':
                useBytesForSize = true;
                break;
            case 'o':
                omitFolders = true;
                break;
            case 'a':
                displayAll = true;
                break;
            case 'p':
                printFullPath = true;
                break;
            case 'l':
                physicalSize = true;
                break;
            case 'L':
                printLabelName = true;
                break;
            case 'Q':
                useQuotes = true;
                break;
            default: /* '?' */
                rc = 1;
                PrintHelp();
                return EX_USAGE;
        }
    }

	argc -= optind;
	argv += optind;

	if(argc) 
	{
		for(i=0; i<argc; i++) 
		{
			if( argc > 1 ) 
			{
				if( i > 0 ) 
				{
					printf("\n");
				}
				printf("%s:\n", argv[i]);
			}
			ListDirectoryContents( argv[i] );
		}
	} 
	else 
	{
        /* If no path is specified as argument, we use the current working directory */
        cwd = getcwd( buf, sizeof(buf) );

		if( !cwd ) 
		{
			fprintf(stderr, "Error getting working directory.\n");
			return(EX_IOERR);
		}
		ListDirectoryContents( cwd );
	}

    return(EX_OK);
}

#pragma mark -


/*//////////////////////////////////////
// Print version and author to stdout
/////////////////////////////////////*/

static void PrintVersion (void)
{
    printf("%s version %s by %s\n", PROGRAM_STRING, VERSION_STRING, AUTHOR_STRING);
}

/*//////////////////////////////////////
// Print help string to stdout
/////////////////////////////////////*/

static void PrintHelp (void)
{
    printf("usage: %s\n", USAGE_STRING);
}

#pragma mark -

/*//////////////////////////////////////
// Iterate through directory and list its items
/////////////////////////////////////*/

static void ListDirectoryContents(char *pathPtr)
{
	char			path[MAX_PATH_LENGTH + MAX_FILENAME_LENGTH + 1];
	DIR				*directory;
	struct dirent 	*dentry;
	char			*sizeStrTot;      // total of files in folder others folders excluded

	if (!pathPtr[0]) 
	{
		fprintf(stderr, "Invalid directory parameter.\n");
		exit(EX_USAGE);
	}

    /* open directory */
    directory = opendir(pathPtr);

    /* if it's invalid, we return with an error */
    if (!directory) 
    {
        perror(pathPtr);
        exit(EX_USAGE);
    }
    
    errno = 0;

    /* iterate through the specified directory's contents */
	while( (dentry = readdir(directory)) ) 
        {
		/* create the file's full path */
		strcpy(path, pathPtr);
		strcat(path, "/");
		strcat(path, (char *)&dentry->d_name);

		ListItem(path, (char *)&dentry->d_name);

		errno = 0;
	}

	// report total of all files in folder other folders size are not included
	
	sizeStrTot = useBytesForSize ? GetSizeString(totalFolderSize) : GetHumanSizeString(totalFolderSize);
	
	printf("                  %s\n","----------------------------------------------" );
	printf("                   %s %s\n", sizeStrTot, "Total Size of Files in Folder" );


	/* report errors and close dir */
	if (errno != 0) 
		perror("readdir(3)");

	if (closedir(directory) == -1) 
		perror("closedir(3)");
}


/*//////////////////////////////////////
// List some item in directory
/////////////////////////////////////*/

static void ListItem (char *path, char *name)
{
    FSRef		fileRef;
    OSErr		err = noErr;
    short		isFldr;

    /* unless the -a paramter is passed, we don't list hidden .* files */
    if (name[0] == '.' && !displayAll) 
            return;

	/* Get file ref to the file or folder pointed to by the path */
    err = FSPathMakeRef((unsigned char *)path, &fileRef, NULL);

    if (err != noErr) 
    {
        if (err != VOL_NOT_FOUND)   // suppress error with files or folders like /.vol or /dev
		fprintf(stderr, "FSPathMakeRef(): Error %d returned when getting file reference from %s\n", err, path);
        return;
    }
    
    /* Check if we're dealing with a folder */
    isFldr = UnixIsFolder(path);
	// printf("isFldr : %d  %s\n", isFldr, path);
	
    if (isFldr == -1)/* an error occurred in stat */
    {
            perror(path);
            return;
    }
    
    /*  if (isFldr)   // it's a folder
    {
        if (!omitFolders)
            ListFolder(name, path, fileRef);
    }
    else // regular file
    {
        if (!foldersOnly) 
            ListFile(name, path, fileRef);
    } */
	
	if (!isFldr)   // it's a regular file
		{
			if (!foldersOnly)
				ListFile(name, path, fileRef);
		}
	 else
		{
			if (isFldr == IS_SYMLINK)
				{
				if (!foldersOnly)
					{
					err = MyFSPathMakeRef ((unsigned char *)path, &fileRef);
					ListFile(name, path, fileRef);
					}
				}
			else
			{
			if (!omitFolders)
				ListFolder(name, path, fileRef);
			}
		}
}


/*//////////////////////////////////////
// Print directory item info for a file
/////////////////////////////////////*/

static void ListFile(char *name, char *path, FSRef fileRef)
{
    /* File manager structures */
    FSSpec		fileSpec;
    FInfo 		finderInfo;

    char		fileType[5];
    char		creatorType[5];
    char		*sizeStr;
    char		quote;
    char		fflagstr[7];
    char		*fileName;
    char		*aliasSrcPath;
    
    UInt64		totalPhysicalSize;
    UInt64		totalLogicalSize;
    // long		size;
	UInt64		size;
	
    short               labelNum;
    OSErr		err = noErr;

    /* retrieve filespec from file ref */
    err = FSGetCatalogInfo (&fileRef, NULL, NULL, NULL, &fileSpec, NULL);
    if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d getting file spec from file reference", err);
        exit(EX_IOERR);
    }

    /* get the finder info */
    err = FSpGetFInfo (&fileSpec, &finderInfo);
    if (err != noErr) 
    {
        fprintf(stderr, "FSpGetFInfo(): Error %d getting file Finder info from file spec", err);
        fprintf(stderr, "%s\n", path);
        exit(EX_IOERR);
    }

    /* ///// Finder flags////// */
    
	/* Is Invisible */
	fflagstr[0] = (finderInfo.fdFlags & kIsInvisible) ? 'I' : '-';

	/* Has Custom Icon */
	fflagstr[1] = (finderInfo.fdFlags & kHasCustomIcon) ? 'C' : '-';

	/* Is Locked */
        fflagstr[2] = (finderInfo.fdFlags & kNameLocked) ? 'L' : '-';

	/* Has Bundle Bit Set */
	fflagstr[3] = (finderInfo.fdFlags & kHasBundle) ? 'B' : '-';

	/* Is Alias */
        fflagstr[4] = (finderInfo.fdFlags & kIsAlias) ? 'A' : '-';

	/* Is Stationery */
	fflagstr[5] = (finderInfo.fdFlags & kIsStationery) ? 'S' : '-';

	fflagstr[6] = '\0';    

    /* ///// File/Creator types ///// */

	/* get file type string */
	OSTypeToStr(finderInfo.fdType, fileType);

	/* get creator type string */
	OSTypeToStr(finderInfo.fdCreator, creatorType);

	/* ///// File Sizes ////// */
    err = GetForkSizes(&fileRef,  &totalLogicalSize, &totalPhysicalSize, forkToDisplay);
    if (err != noErr) 
    {
        fprintf(stderr, "GetForkSizes(): Error %d getting size of file forks\n", err);
        exit(EX_IOERR);
    }

    /*////////////////////////////
    // This needs to be fixed in the future
    // The size of files larger than 4GB is not displayed correctly due
    // due the use of a long int to store file size
    ////////////////////////////*/
    size = physicalSize ? totalPhysicalSize : totalLogicalSize;

    size = totalLogicalSize;
	
	totalFolderSize += size;   // update the total with the current file
	
    sizeStr = useBytesForSize ? GetSizeString(size) : GetHumanSizeString(size);

    /* if the -Q option is specified */
    quote = useQuotes ? '"' : ' ';

    /* if the -p option is specified */
    fileName = printFullPath ? path : name;
    
    
    // Print label
    if (printLabelName)
    {
            labelNum = GetLabelNumber(finderInfo.fdFlags);
            printf("%s ", (char *)&labelNames[labelNum]);
    }
    /* /////// Print output for this directory item //////// */
    if (finderInfo.fdFlags & kIsAlias)
    {
        aliasSrcPath = GetPathOfAliasSource(path);
        printf("%s  %4s %4s  %s %c%s%c-->%c%s%c\n", fflagstr, fileType, creatorType, sizeStr, quote, fileName, quote, quote, aliasSrcPath, quote);
    }
    else
        printf("%s  %4s %4s  %s %c%s%c\n", fflagstr, fileType, creatorType, sizeStr, quote, fileName, quote);
}

/*//////////////////////////////////////
// Print directory item info for a folder
/////////////////////////////////////*/
static void ListFolder (char *name, char *path, FSRef fileRef)
{
    char	quote;
    long	valence;
    char	*numFilesStr;
    char	*fileName;
    char        fflagstr[7];
    short       labelNum;
    const char	*sizeStr;
    const char	*humanSizeStr = "    -   ";
    const char	*byteSizeStr  = "           -  ";
    FSSpec      fileSpec;
    DInfo       dInfo;//directory information

    /*
	 * Retrieve number of files within folder from the 
         * FSCatalog record
    */
    valence = GetNumFilesInFolder(&fileRef);
    if (valence == -1)/* error */
    {
        fprintf(stderr, "%s: Error getting number of files in folder\n", name);
        return;
    }

    /* generate a suitable-length string from this number */
    numFilesStr = GetNumFilesString(valence);
    if (!numFilesStr) 
    {
        fprintf(stderr, "%s: Error getting number of files in folder\n", name);
        return;
    }
    
    /* modify according to the options specified */
	fileName = printFullPath ? path : name;

	sizeStr = useBytesForSize ? byteSizeStr : humanSizeStr;

	quote = useQuotes ? '"' : ' ';

        FSGetCatalogInfo (&fileRef, NULL, NULL, NULL, &fileSpec, NULL);
        FSpGetDInfo(&fileSpec, &dInfo);

                
        /* Is Invisible */
	fflagstr[0] = (dInfo.frFlags & kIsInvisible) ? 'I' : '-';

	/* Has Custom Icon */
	fflagstr[1] = (dInfo.frFlags & kHasCustomIcon) ? 'C' : '-';

	/* Is Locked */
        fflagstr[2] = (dInfo.frFlags & kNameLocked) ? 'L' : '-';

	/* Has Bundle Bit Set */
	fflagstr[3] = (dInfo.frFlags & kHasBundle) ? 'B' : '-';

	/* Is Alias */
        fflagstr[4] = (dInfo.frFlags & kIsAlias) ? 'A' : '-';

	/* Is Stationery */
	fflagstr[5] = (dInfo.frFlags & kIsStationery) ? 'S' : '-';

	fflagstr[6] = '\0';

        
        
        // get label
        if (printLabelName)
        {
            labelNum = GetLabelNumber(dInfo.frFlags);
            printf("%s ", (char *)&labelNames[labelNum]);
        }
        
        //print out line
	printf("%s %s     %s %c%s/%c\n", fflagstr, numFilesStr, sizeStr, quote, fileName, quote);
        
        return;
    
}

#pragma mark -

/*//////////////////////////////////////
// Generate a string for number of files within a folder
/////////////////////////////////////*/

static char* GetNumFilesString (long numFiles)
{
    static char	numFilesStr[11];
    
    /* there can't be less than 0 files in a folder...duh */
    if (numFiles < 0)
        return NULL;
    
    /* we list a maximum of 9999 files within folder
       this needs to be fixed at some point */
    if (numFiles > 9999)
        numFiles = 9999;
    
    /* create a string that contains just the number */
    sprintf(numFilesStr, "%4d items", (int)numFiles);

    return numFilesStr;
}


/*//////////////////////////////////////
// Get the number of files contained within
// the folder pointed to by an FSRef
/////////////////////////////////////*/
static long GetNumFilesInFolder (FSRef *fileRef)
{
    OSErr		err;
    FSCatalogInfo	catInfo;
    
    /* access the FSCatalog record to get the number of files */
    err = FSGetCatalogInfo(fileRef, kFSCatInfoValence, &catInfo, NULL, NULL, NULL);

    if (err)
        return(-1);

    return (catInfo.valence);

}

/*//////////////////////////////////////
// Check parameter for the -f option
// and see if it's valild.  If so, then
// return the setting
/////////////////////////////////////*/
short GetForkParameterFromString (char *str)
{
    if (strlen(str) != 4)
    {
        fprintf(stderr, "Illegal parameter: %s\nYou must specify one of the following: rsrc, data, both", str);
        exit(EX_USAGE);
    }

    if (!strcmp((char *)&RSRC_FORK_PARAM_NAME, str)) 
    {
        /* resource fork specified - 'rsrc' */
        return DISPLAY_FORK_RSRC;
    } 
    else if (!strcmp((char *)&DATA_FORK_PARAM_NAME, str)) 
    {
        /* data fork specified - 'data' */
        return DISPLAY_FORK_DATA;
    } 
    else if (!strcmp((char *)&BOTH_FORK_PARAM_NAME, str)) 
    {
        /* both forks specified - 'both' */
        return DISPLAY_FORK_BOTH;
    }

    fprintf(stderr, "Illegal parameter: %s\nYou must specify one of the following: rsrc, data, both\n", str);
    exit(EX_USAGE);

    /* to appease the compiler */
    return 0;
}


/*//////////////////////////////////////
// Generate file size string in bytes
/////////////////////////////////////*/

// static char* GetSizeString (long size)
static char* GetSizeString (UInt64 size)
{
    // static char	sizeStr[15];
	static char	sizeStr[17];
	
    // sprintf(sizeStr, "%12d B", (int)size);
    sprintf(sizeStr, "%15llu B",size );
    return sizeStr;
}

/*//////////////////////////////////////
// Generate file size string in human readable format
/////////////////////////////////////*/

static char* GetHumanSizeString (UInt64 size)
{
    static char	humanSizeStr[11];

    if (size < 1024) 
    {
                /* bytes */
        sprintf(humanSizeStr, "   %5d  B", (int)size);
    } 
    else if (size < 1048576) {
            /* kbytes */
        sprintf(humanSizeStr, "   %5.1f KB", size / 1024.0);
    } 
    else if (size < 1073741824) 
    {
            /* megabytes */
        sprintf(humanSizeStr, "   %5.1f MB", size / 1048576.0);
    } 
    else 
    {
		/* gigabytes */
		sprintf(humanSizeStr, "   %5.1f GB", size / 1073741824.0);
    }

    return (humanSizeStr);
}


/*//////////////////////////////////////////
// Function to get size of file forks
//////////////////////////////////////////*/

static OSErr GetForkSizes (const FSRef *fileRef,  UInt64 *totalLogicalForkSize, UInt64 *totalPhysicalForkSize, short fork)  
{
    /*
        the fork paramater can be one of three possible values
        
        0 - Both forks
        1 - Data fork
        2 - Resource fork

    */

    OSErr   		err;
    CatPositionRec 	forkIterator;
    
    SInt64   		forkLogicalSize = (SInt64)NULL;
    SInt64   		*forkLogicalSizePtr;
    UInt64   		forkPhysicalSize = (UInt64)NULL;
    UInt64   		*forkPhysicalSizePtr;

    HFSUniStr255	forkName;
    char		forkStr[255];

    /* if logical fork size is needed */
    if (totalLogicalForkSize) 
    {
        *totalLogicalForkSize = 0;
        forkLogicalSizePtr = &forkLogicalSize;
    } 
    else 
    {
        forkLogicalSizePtr = NULL;
    }
    
    /* if physical fork size is needed */
    if (totalPhysicalForkSize) 
    {
        *totalPhysicalForkSize = 0;
        forkPhysicalSizePtr = &forkPhysicalSize;
    } 
    else 
    {
        forkPhysicalSizePtr = NULL;
    }
    
    /* Iterate through the file's forks and get their combined size */
    forkIterator.initialize = 0;

    do 
    {
        err = FSIterateForks(fileRef, &forkIterator, &forkName, forkLogicalSizePtr, forkPhysicalSizePtr);
        if (noErr == err) 
        {
            HFSUniPStrToCString(&forkName, (char *)&forkStr);

            /* if it's the resource fork we're doing now */
            if (!strcmp((char *)&"RESOURCE_FORK", (char *)&forkStr)) 
            {
                /* if we're not just displaying the data fork */
                if (fork != DISPLAY_FORK_DATA) 
                {
                    if (totalLogicalForkSize) 
                    {
                        *totalLogicalForkSize += forkLogicalSize;
                    }
                    if (totalPhysicalForkSize) 
                    {
                        *totalPhysicalForkSize += forkPhysicalSize;
                    }
                }
            }
            else/* must be the data fork, then */
            {
                if (fork != DISPLAY_FORK_RSRC) 
                {
                    if (totalLogicalForkSize) 
                    {
                        *totalLogicalForkSize += forkLogicalSize;
                    }
                    if (totalPhysicalForkSize) 
                    {
                        *totalPhysicalForkSize += forkPhysicalSize;
                    }
                }
            }
        }
        
    } while (err == noErr);    
    
    /* errFSNoMoreItems is not serious, we report other errors */
    if (err && err != errFSNoMoreItems) 
    {
        return err;
    } 
    else 
    {
        err = noErr;
    }
    
    return(err);
}




#pragma mark -


/*//////////////////////////////////////
// Transform OSType into a C string
// An OSType is just a 4 character string
// stored as a long integer
/////////////////////////////////////*/
void OSTypeToStr(OSType aType, char *aStr)
{
	aStr[0] = (aType >> 24) & 0xFF;
	aStr[1] = (aType >> 16) & 0xFF;
	aStr[2] = (aType >> 8) & 0xFF;
	aStr[3] = aType & 0xFF;
        aStr[4] = 0;
}

/*//////////////////////////////////////
// Check if file in designated path is folder
// This is faster than the File-Manager based
// function above
/////////////////////////////////////*/
static int UnixIsFolder (char *path)
{
    struct stat filestat;
    short err;
    short i;      // file type 0 = regular 1 = folder
	
    // err = stat(path, &filestat);
	err = lstat(path, &filestat);   // use lstat for symlinks
	
	if (err == -1)
        return err;

    // return (S_ISREG(filestat.st_mode) != 1);
	
	i = (S_ISREG(filestat.st_mode) != 1);	// only 0 for regular files
		
	if ( !i )
	 return ( i );
	 	
	i = (S_ISLNK(filestat.st_mode) != 1);	// only 0 for symlinks
	
	if ( !i )
	 {
	   return ( IS_SYMLINK );
	 }
	else
	 {
	   return ( i );
	 }
}

/**************************************************************************************/

  /* Due to a bug in the X File Manager, 2489632,			*/
  /* FSPathMakeRef doesn't handle symlinks properly.  It    */
  /* automatically resolves it and returns an FSRef to the  */
  /* symlinks target, not the symlink itself.  So this is a */
  /* little workaround for it...							*/
  /*														*/
  /* We could call lstat() to find out if the object is a   */
  /* symlink or not before jumping into the guts of the		*/
  /* routine, but its just as simple, and fast when working */
  /* on a single item like this, to send everything through */
  /* this routine											*/
  
static OSErr MyFSPathMakeRef( const unsigned char *path, FSRef *ref )
{
  FSRef      tmpFSRef;
  char      tmpPath[ MAX_PATH_LENGTH ],
          *tmpNamePtr;
  OSErr      err;
  
          /* Get local copy of incoming path          */
  strcpy( tmpPath, (char*)path );

          /* Get the name of the object from the given path  */
          /* Find the last / and change it to a '\0' so    */
          /* tmpPath is a path to the parent directory of the  */
          /* object and tmpNamePtr is the name        */
  tmpNamePtr = strrchr( tmpPath, '/' );
  if( *(tmpNamePtr + 1) == '\0' )
  {        /* in case the last character in the path is a /  */
    *tmpNamePtr = '\0';
    tmpNamePtr = strrchr( tmpPath, '/' );
  }
  *tmpNamePtr = '\0';
  tmpNamePtr++;
  
          /* Get the FSRef to the parent directory      */
  err = FSPathMakeRef( (unsigned char*)tmpPath, &tmpFSRef, NULL );
  if( err == noErr )
  {        
          /* Convert the name to a Unicode string and pass it  */
          /* to FSMakeFSRefUnicode to actually get the FSRef  */
          /* to the object (symlink)              */
    HFSUniStr255  uniName;
    err = ConvertCStringToHFSUniStr( tmpNamePtr, &uniName );
    if( err == noErr )
      err = FSMakeFSRefUnicode( &tmpFSRef, uniName.length, uniName.unicode, kTextEncodingUnknown, &tmpFSRef );
  }
  
  if( err == noErr )
    *ref = tmpFSRef;
  
  return err;
}

/******************************************************************************/

static OSErr ConvertCStringToHFSUniStr(const char* cStr, HFSUniStr255 *uniStr)
{
	OSErr err = noErr;
	CFStringRef tmpStringRef = CFStringCreateWithCString( kCFAllocatorDefault, cStr, kCFStringEncodingMacRoman );
	if( tmpStringRef != NULL )
	{
		if( CFStringGetCString( tmpStringRef, (char*)uniStr->unicode, sizeof(uniStr->unicode), kCFStringEncodingUnicode ) )
			uniStr->length = CFStringGetLength( tmpStringRef );
		else
			err = kCouldNotGetStringData;
			
		CFRelease( tmpStringRef );
	}
	else
		err = kCouldNotCreateCFString;
	
	return err;
}

/*//////////////////////////////////////
// HFSUniStr255 converted to null-terminated C string
// For some inexplicable reason, there seems to be no
// convenient Carbon function to do this without
// first converting to a Core Foundation string
/////////////////////////////////////*/
static void HFSUniPStrToCString (HFSUniStr255 *uniStr, char *cstr)
{
    CFStringRef		cfStr;

    cfStr = CFStringCreateWithCharacters(kCFAllocatorDefault,uniStr->unicode,uniStr->length);
    
    CFStringGetCString(cfStr, cstr, 255, kCFStringEncodingUTF8);
}


/*//////////////////////////////////////
// On being passed the path to a Mac OS alias,
// returns path to the original file to which
// it refers.  Returns null if file not found.
/////////////////////////////////////*/
static char* GetPathOfAliasSource (char *path)
{
    OSErr	err = noErr;
    static char	srcPath[2000];
    FSRef	fileRef;
    Boolean	isAlias, isFolder;

    //get file reference from path given
    err = FSPathMakeRef (path, &fileRef, NULL);
    if (err != noErr)
    {
        fprintf(stderr, "Error getting file spec from path.\n");
        return 0;
    }
    
    //make sure we're dealing with an alias
    err = FSIsAliasFile (&fileRef, &isAlias, &isFolder);
    if (err != noErr)
    {
        //printf("Error determining alias properties.\n");
        return NULL;
    }
    if (!isAlias)
    {
        //printf("%s: Not an alias.\n", argv[1]);
        return NULL;
    }
    
    //resolve alias --> get file reference to file
    err = FSResolveAliasFile (&fileRef, TRUE, &isFolder, &isAlias);
    if (err != noErr)
    {
        //printf("Error resolving alias.\n");
        return NULL;
    }
    
    //get path to file that alias points to
    err = FSMakePath(fileRef, (char *)&srcPath, strlen(srcPath));
    if (err != noErr)
    {
        return NULL;
    }
    
    return ((char *)&srcPath);
}


/*//////////////////////////////////////
// Creates POSIX path string from FSRef
/////////////////////////////////////*/
static OSStatus FSMakePath(FSRef fileRef, UInt8 *path, UInt32 maxPathSize)
{
    OSStatus result;
 
    result = FSRefMakePath(&fileRef, path, 2000);

    return ( result );
}

/*//////////////////////////////////////
// Returns directory info structure of 
// file spec
/////////////////////////////////////*/
static OSErr FSpGetDInfo(const FSSpec* fileSpec, DInfo *dInfo)
{
	CInfoPBRec	infoRec = {0};
	OSErr		err = noErr;

	infoRec.hFileInfo.ioNamePtr = (unsigned char *)fileSpec->name;
	infoRec.hFileInfo.ioVRefNum = fileSpec->vRefNum;
	infoRec.hFileInfo.ioDirID = fileSpec->parID;
	err = PBGetCatInfoSync(&infoRec);
	if (err == noErr) 
        {
                *dInfo = infoRec.dirInfo.ioDrUsrWds;
	}

	return err;
}

/*//////////////////////////////////////
// Checks bits 1-3 of fdFlags and frFlags
// values in FInfo and DInfo structs
// and gets the relevant color/number
/////////////////////////////////////*/
static short GetLabelNumber (short flags)
{
        /* Is Orange */
	if (flags & 2 && flags & 8 && flags & 4)
            return 2;

        /* Is Red */
	if (flags & 8 && flags & 4)
            return 1;

        /* Is Yellow */
	if (flags & 8 && flags & 2)
            return 3;
        
        /* Is Blue */
	if (flags & 8)
            return 5;
        
        /* Is Purple */
	if (flags & 2 && flags & 4)
            return 6;
        
        /* Is Green */
	if (flags & 4)
            return 4;
        
        /* Is Gray */
	if (flags & 2)
            return 7;

    return 0;
}