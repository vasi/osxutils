/*
    lsmac - ls-like utility for viewing MacOS file meta-data
    Copyright (C) 2003 Sveinbjorn Thordarson <sveinbt@hi.is>

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
    0.4 -   * made internal methods static (reduced file size by more than 50%)
            * removed dynamic memory allocation and other performance enhancements
            * Define KB, MB and GB as 2^10, 2^20 and 2^30 bytes, respectively
            * List multiple directories given as command line arguments

    0.3 -   * lsmac can now be told whether to list the size of resource fork, data fork or both.  Both is default
            * -Q option: names/paths printed within quotation marks
            * lsmac now displays file size in human readable format by default
            * fixed error that occurred when listing root directory
            * -F option: only folders are listed
            * Num. of files within folders is now listed by default
            * Some minor optimizations (f.e. stat used for determining directories, free & malloc instead of NewPtr & DisposePtr, etc.)
            * Dynamic memory allocation for


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
        
        * Bug - errors may occur when listing folders containing more than 9999 files.
        * Handle symlinks properly, i.e. not just list them as if they were the files they pointed to
        * Bug - lsmac reports error when traversing /dev.
        * Bug - Does not display size correctly for files larger than 4GB or so. 64bit int problem.
        * Try to optimize speed.  lsmac default output is 3x-4x slower than ls -l
        * Optimize memory usage
        * Accept more than one argument to list, and, on a similar note...
            * Make lsmac accept files as command line parameter, and then just print the info about those files
        * Implement calculation of folder sizes - recursively scan through hierarchy and add up total size
        * Display folder flags from the DInfo structure
        * There is other Mac file meta-data not available via FSpGetFInfo() which should also be listed
        * -x option: list file suffixes in a seperate column
        * List total size of all files listed on top, akin to ls -l

*/


#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <Carbon/Carbon.h>
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

static char* GetSizeString (long size);
static char* GetHumanSizeString (long size);
static OSErr GetForkSizes (const FSRef *fileRef,  UInt64 *totalLogicalForkSize, UInt64 *totalPhysicalForkSize, short fork);

static void OSTypeToStr(OSType aType, char *aStr);
/*
static Boolean IsFolder (FSRef *fileRef);
*/
static int UnixIsFolder (char *path);
static void HFSUniPStrToCString (HFSUniStr255 *uniStr, char *cstr);


/*///////Definitions///////////////////*/

#define		PROGRAM_STRING  	"lsmac"
#define		VERSION_STRING		"0.4"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson"

#define		MAX_PATH_LENGTH		4096
#define		MAX_FILENAME_LENGTH	256

#define		OPT_STRING		"vhf:FsboaplQ"

#define		DISPLAY_FORK_BOTH	0
#define		DISPLAY_FORK_DATA	1
#define		DISPLAY_FORK_RSRC	2

#define		BOTH_FORK_PARAM_NAME	"both"
#define		DATA_FORK_PARAM_NAME	"data"
#define		RSRC_FORK_PARAM_NAME	"rsrc"



/*///////Globals///////////////////*/

static int		omitFolders = false;
static int		displayAll = false;
static int		printFullPath = false;
static int		useBytesForSize = false;
static int		physicalSize = false;
static int		forkToDisplay = DISPLAY_FORK_BOTH;
static int		useQuotes = false;
/* static int		listSuffix = false; */
static int		foldersOnly = false;

/*
static int		calcFolderSizes = false;
static int		calcNumFolderItems = false;
*/

/*
    Command line options

    v - version
    h - help
    s - display size
    b - display size in bytes
    o - omit folders - will not omit folder aliases
    a - display all (including hidden . files)
    p - print full file path
    l - when printing size, print physical size, not logical size
    
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
	char		buf[MAX_PATH_LENGTH];
	char		*cwd;

    while ( (optch = getopt(argc, argv, optstring)) != -1)
    {
        switch(optch)
        {
            case 'v':
                PrintVersion();
                return 0;
                break;
            case 'h':
                PrintHelp();
                return 0;
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
            /* not quite ready for this yet */
           /* case 'x':
                listSuffix = true;
                break;*/
            case 'a':
                displayAll = true;
                break;
            case 'p':
                printFullPath = true;
                break;
            case 'l':
                physicalSize = true;
                break;
            case 'Q':
                useQuotes = true;
                break;
            /*case 'c':
                calculateFolderSizes = true;
                break;
            case 'i':
                calcNumFolderItems = true;
                break;*/
            default: /* '?' */
                rc = 1;
                PrintHelp();
                return 0;
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
			fputs("Error getting working directory.\n", stderr);
			return( 1 );
		}

		ListDirectoryContents( cwd );
	}

    return( 0 );
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
    /*printf("%s version %s - ls-like program for viewing Mac meta-data\n", PROGRAM_STRING, VERSION_STRING);*/
    printf("usage: lsmac [-%s] directory\n", OPT_STRING);
}

#pragma mark -

/*//////////////////////////////////////
// Iterate through directory and list its items
/////////////////////////////////////*/

static void ListDirectoryContents(char *pathPtr)
{
	char		path[MAX_PATH_LENGTH + MAX_FILENAME_LENGTH + 1];
	DIR			*directory;
	struct dirent 	*dentry;

    /*check if path given as argument is too long to handle */
   /* if (len > MAX_PATH_LENGTH)
    {
        fputs("Path is too long for lsmac.  Recompile with larger MAX_PATH_LENGTH set.\n", stderr);
        ExitToShell();
    }*/

	if (!pathPtr[0]) {
		fputs("Invalid directory parameter.\n", stderr);
		ExitToShell();
	}

    /* open directory */
    directory = opendir(pathPtr);

    /* if it's invalid, we return with an error */
    if (!directory) {
        perror(pathPtr);
        ExitToShell();
    }
    
    errno = 0;

    /* iterate through the specified directory's contents */
	while( (dentry = readdir(directory)) ) {
		/* create the file's full path */
		strcpy(path, pathPtr);
		strcat(path, "/");
		strcat(path, (char *)&dentry->d_name);

		ListItem(path, (char *)&dentry->d_name);

		errno = 0;
	}

	/* report errors and close dir */
	if (errno != 0) {
		perror("readdir(3)");
	}

	if (closedir(directory) == -1) {
		perror("closedir(3)");
	}
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
    if (name[0] == '.' && !displayAll) {
        return;
	}

	/* Get file ref to the file or folder pointed to by the path */
    err = FSPathMakeRef((unsigned char *)path, &fileRef, NULL);

    if (err != noErr) {
        printf("FSPathMakeRef(): Error %d returned when getting file reference from %s\n", err, path);
        /* ExitToShell(); */
        return;
    }
    
    /* Check if we're dealing with a folder */
    isFldr = UnixIsFolder(path);
    if (isFldr == -1)/* an error occurred in stat */
    {
            perror(path);
            return;
    }
    
    if (isFldr)/* it's a folder */
    {
        if (!omitFolders) {
			ListFolder(name, path, fileRef);
		}
    }
    else/* regular file */
    {
        if (!foldersOnly) {
			ListFile(name, path, fileRef);
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

    UInt64		totalPhysicalSize;
    UInt64		totalLogicalSize;
    long		size;
    OSErr		err = noErr;

    /* retrieve filespec from file ref */
    err = FSGetCatalogInfo (&fileRef, NULL, NULL, NULL, &fileSpec, NULL);
    if (err != noErr) {
        printf("FSGetCatalogInfo(): Error %d getting file spec from file reference", err);
        ExitToShell();
    }

    /* get the finder info */
    err = FSpGetFInfo (&fileSpec, &finderInfo);
    if (err != noErr) {
        printf("FSpGetFInfo(): Error %d getting file finder info from file spec", err);
        ExitToShell();
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
    if (err != noErr) {
        printf("GetForkSizes(): Error %d getting size of file forks\n", err);
        ExitToShell();
    }

    /*////////////////////////////
    // This needs to be fixed in the future
    // The size of files larger than 4GB is not displayed correctly due
    // due the use of a long int to store file size
    ////////////////////////////*/
	size = physicalSize ? totalPhysicalSize : totalLogicalSize;

    size = totalLogicalSize;

    sizeStr = useBytesForSize ? GetSizeString(size) : GetHumanSizeString(size);

	/* if the -Q option is specified */
	quote = useQuotes ? '"' : ' ';

	/* if the -p option is specified */
	fileName = printFullPath ? path : name;
    
    /* if the -x option is specified */
    /*if (listSuffix)
    {
        suffixPtr = GetFileNameSuffix(name);
        
        if (suffixPtr == NULL)
        {
            strcpy(&suffixStr, "     ");
        }
        else
        {
            strcpy(&suffixStr, GetFileNameSuffix(fileName));
            name[strlen(fileName)-strlen(suffixStr)] = NULL;
        }
    }*/
    

    /* /////// Print output for this directory item //////// */

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
    const char	*sizeStr;
    const char	*humanSizeStr = "    -   ";
    const char	*byteSizeStr  = "           -  ";

    /*
	 * Retrieve number of files within folder from the 
     * FSCatalog record
	 */
    valence = GetNumFilesInFolder(&fileRef);
    if (valence == -1)/* error */
    {
        printf("%s: Error getting number of files in folder\n", name);
        return;
    }

    /* generate a suitable-length string from this number */
    numFilesStr = GetNumFilesString(valence);
    if (!numFilesStr) {
        printf("%s: Error getting number of files in folder\n", name);
        return;
    }
    
    /* modify according to the options specified */
	fileName = printFullPath ? path : name;

	sizeStr = useBytesForSize ? byteSizeStr : humanSizeStr;

	quote = useQuotes ? '"' : ' ';

	printf("------ %s  %s %c%s/%c\n", numFilesStr, sizeStr, quote, fileName, quote);
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

    if (err) {
        return(-1);
	}

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
        printf("Illegal parameter: %s\nYou must specify one of the following: rsrc, data, both", str);
        ExitToShell();
    }

	if (!strcmp((char *)&RSRC_FORK_PARAM_NAME, str)) {
		/* resource fork specified - 'rsrc' */
		return DISPLAY_FORK_RSRC;
    } else if (!strcmp((char *)&DATA_FORK_PARAM_NAME, str)) {
		/* data fork specified - 'data' */
        return DISPLAY_FORK_DATA;
    } else if (!strcmp((char *)&BOTH_FORK_PARAM_NAME, str)) {
		/* both forks specified - 'both' */
		return DISPLAY_FORK_BOTH;
	}

	printf("Illegal parameter: %s\nYou must specify one of the following: rsrc, data, both\n", str);
	ExitToShell();

	/* to appease the compiler */
	return 0;
}


/*//////////////////////////////////////
// Generate file size string in bytes
/////////////////////////////////////*/

static char* GetSizeString (long size)
{
    static char	sizeStr[15];

    sprintf(sizeStr, "%12d B", (int)size);
    
    return sizeStr;
}

/*//////////////////////////////////////
// Generate file size string in human readable format
/////////////////////////////////////*/

static char* GetHumanSizeString (long size)
{
    static char	humanSizeStr[11];

    if (size < 1024) {
		 /* bytes */
		sprintf(humanSizeStr, "%5d  B", (int)size);
	} else if (size < 1048576) {
		/* kbytes */
		sprintf(humanSizeStr, "%5.1f KB", size / 1024.0);
	} else if (size < 1073741824) {
		/* megabytes */
        sprintf(humanSizeStr, "%5.1f MB", size / 1048576.0);
    } else {
		/* gigabytes */
		sprintf(humanSizeStr, "%5.1f GB", size / 1073741824.0);
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
    
    SInt64   		forkLogicalSize = NULL;
    SInt64   		*forkLogicalSizePtr;
    UInt64   		forkPhysicalSize = NULL;
    UInt64   		*forkPhysicalSizePtr;

    HFSUniStr255	forkName;
    char		forkStr[255];

    /* if logical fork size is needed */
    if (totalLogicalForkSize) {
        *totalLogicalForkSize = 0;
        forkLogicalSizePtr = &forkLogicalSize;
    } else {
        forkLogicalSizePtr = NULL;
    }
    
    /* if physical fork size is needed */
    if (totalPhysicalForkSize) {
        *totalPhysicalForkSize = 0;
        forkPhysicalSizePtr = &forkPhysicalSize;
    } else {
        forkPhysicalSizePtr = NULL;
    }
    
    /* Iterate through the file's forks and get their combined size */
    forkIterator.initialize = 0;

    do {
        err = FSIterateForks(fileRef, &forkIterator, &forkName, forkLogicalSizePtr, forkPhysicalSizePtr);
        if (noErr == err) {
            HFSUniPStrToCString(&forkName, (char *)&forkStr);

            /* if it's the resource fork we're doing now */
            if (!strcmp((char *)&"RESOURCE_FORK", (char *)&forkStr)) {
                /* if we're not just displaying the data fork */
                if (fork != DISPLAY_FORK_DATA) {
                    if (totalLogicalForkSize) {
                        *totalLogicalForkSize += forkLogicalSize;
					}
                    if (totalPhysicalForkSize) {
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

/*//////////////////////////////////////
// Cut a file suffix of up to 4 characters
// from listed file names and return it
// It is then listed in a seperate column
// because of the -x option
/////////////////////////////////////*/

/*char* GetFileNameSuffix (char *name)
{
    short	len = strlen(name);
    short	i;
    
    for (i = 5; i > 0; i--)
    {
        if (name[len-i] == '.')
        {
            return (&name[len-i]);
        }
    }
    
    return NULL;
}*/



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
// Check if FSRef is a folder
/////////////////////////////////////*/
/*Boolean IsFolder (FSRef *fileRef)
{
	FSCatalogInfo 	catalogInfo;
	OSErr			err;

	err = FSGetCatalogInfo(fileRef, kFSCatInfoNodeFlags, &catalogInfo, NULL, NULL, NULL);
	if ((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) == kFSNodeIsDirectoryMask )
		return true;
	else
		return false;
}*/

/*//////////////////////////////////////
// Check if file in designated path is folder
// This is faster than the File-Manager based
// function above
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

/*//////////////////////////////////////
// HFSUniStr255 converted to null-terminated C string
// For some inexplicable reason, there seems to be no
// convenient Apple function to do this without
// first converting to a Core Foundation string
/////////////////////////////////////*/
static void HFSUniPStrToCString (HFSUniStr255 *uniStr, char *cstr)
{
    CFStringRef		cfStr;

    cfStr = CFStringCreateWithCharacters(kCFAllocatorDefault,uniStr->unicode,uniStr->length);
    
    CFStringGetCString(cfStr, cstr, 255, kCFStringEncodingUTF8);
}
