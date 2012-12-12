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

//  I wrote this program because I found the Apple-supplied command line tools with the 
//  Developer Tools to be rather poor and needed a convenient way to view classic Mac file meta-data.
//  I'd appreciate being notified of any changes/improvements made to the lsmac source code 
//  so I can add them to my own releases.  Thanks and enjoy --  Sveinbjorn Thordarson <sveinbt@.hi.is>


/*  CHANGES

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

/////////Prototypes/////////////////////

void PrintVersion (void);
void PrintHelp (void);

void ListDirectoryContents (char *arg);
void ListItem (char *path, char *name);
void ListFile (char *name, char *path, FSRef fileRef);
void ListFolder (char *name, char *path, FSRef fileRef);

char* GetNumFilesString (long numFiles);
long GetNumFilesInFolder (FSRef *fileRef);

short GetForkParameterFromString (char *str);

char* GetSizeString (long size);
char* GetHumanSizeString (long size);
char* GetSISizeString (long size);
OSErr GetForkSizes (const FSRef *fileRef,  UInt64 *totalLogicalForkSize, UInt64 *totalPhysicalForkSize, short fork);

char* OSTypeToStr(OSType aType);
//Boolean IsFolder (FSRef *fileRef);
short UnixIsFolder (char *path);
void HFSUniPStrToCString (HFSUniStr255 *uniStr, char *cstr);


/////////Definitions/////////////////////

#define		PROGRAM_STRING  	"lsmac"
#define		VERSION_STRING		"0.3.1"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson"

#define		MAX_PATH_LENGTH		4096
#define		MAX_FILENAME_LENGTH	256

#define		OPT_STRING		"vhf:FsBboaplQ"


#define		DISPLAY_FORK_BOTH	0
#define		DISPLAY_FORK_DATA	1
#define		DISPLAY_FORK_RSRC	2

#define		BOTH_FORK_PARAM_NAME	"both"
#define		DATA_FORK_PARAM_NAME	"data"
#define		RSRC_FORK_PARAM_NAME	"rsrc"



/////////Globals/////////////////////

enum		byteType {NORMAL, BASE10, BASE2};
enum 		byteType displaySize = BASE2;
//short		useBytesForSize = false; // base10
short		physicalSize = false;

short		omitFolders = false;
short		displayAll = false;
short		printFullPath = false;
short		forkToDisplay = DISPLAY_FORK_BOTH;
short		useQuotes = false;
short		listSuffix = false;
short		foldersOnly = false;

//short		calcFolderSizes = false;
//short		calcNumFolderItems = false;

/*
    Command line options

    v - version
    h - help
    B - display size as 10^X
    b - display size as y
    s - display size as 2^z
    o - omit folders - will not omit folder aliases
    a - display all (including hidden . files)
    p - print full file path
    l - when printing size, print physical size, not logical size
    
    [-f fork] - select which fork to print size of
    
    c - calculate folder sizes 				** NOT IMPLEMENTED YET **
    i - calculate number of files within folders 	** NOT IMPLEMENTED YET **

*/

////////////////////////////////////////
// Main program function
///////////////////////////////////////
int main (int argc, const char *argv[]) 
{
    int			rc;
    int			optch;
    static char		optstring[] = OPT_STRING;
    
    while ( (optch = getopt(argc, (char * const *)argv, optstring)) != -1)
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
                displaySize = NORMAL; // display size as x
                break;
            case 's':
                displaySize = BASE2; // display size as 2^y
                break;
            case 'B':
                displaySize = BASE10; // display size as 10^z
                break;
            case 'o':
                omitFolders = true;
                break;
            // not quite ready for this yet
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
            default: // '?'
                rc = 1;
                PrintHelp();
                return 0;
        }
    }
    
    ListDirectoryContents((char *)argv[optind]);

    return 0;
}

#pragma mark -


////////////////////////////////////////
// Print version and author to stdout
///////////////////////////////////////

void PrintVersion (void)
{
    printf("%s version %s by %s\n", PROGRAM_STRING, VERSION_STRING, AUTHOR_STRING);
}

////////////////////////////////////////
// Print help string to stdout
///////////////////////////////////////

void PrintHelp (void)
{
    //printf("%s version %s - ls-like program for viewing Mac meta-data\n", PROGRAM_STRING, VERSION_STRING);
    printf("usage: lsmac [-%s] directory\n", OPT_STRING);
}

#pragma mark -

////////////////////////////////////////
// Iterate through directory and list its items
///////////////////////////////////////

void ListDirectoryContents (char *arg)
{
    char		*buf;//[ MAX_PATH_LENGTH ];
    char		*path;//[ MAX_PATH_LENGTH + MAX_FILENAME_LENGTH + 1];
    char		*pathPtr;
    DIR			*directory;
    struct dirent 	*dentry;
    
    //check if path given as argument is too long to handle
   /* if (len > MAX_PATH_LENGTH)
    {
        fputs("Path is too long for lsmac.  Recompile with larger MAX_PATH_LENGTH set.\n", stderr);
        ExitToShell();
    }*/
    
    //allocate memory for path strings
    buf = malloc(MAX_PATH_LENGTH);
    if (buf == NULL)
    {
        printf("Failed to allocate %d bytes of memory for 'buf'.\n", MAX_PATH_LENGTH);
        ExitToShell();
    }
    path = malloc(MAX_PATH_LENGTH + MAX_FILENAME_LENGTH + 1);
    if (path == NULL)
    {
        printf("Failed to allocate %d bytes of memory for 'path'.\n", MAX_PATH_LENGTH + MAX_FILENAME_LENGTH + 1);
        ExitToShell();
    }
    
    //If no path is specified as argument, we use the current working directory
    if (arg == NULL)
    {
        
        pathPtr = getcwd((char *)buf, MAX_PATH_LENGTH);
        
        if (pathPtr == NULL)
        {
            fputs("Error getting working directory.  Perhaps there is insufficient memory.\n", stderr);
            ExitToShell();
        }
    }
    else//use the path given as argument
    {
        strcpy((char *)buf, arg);
        if (strlen((char *)buf) == NULL)
        {
            fputs("Invalid directory parameter.\n", stderr);
            ExitToShell();
        }
    }

    //open directory
    directory = opendir(buf);
    
    //if it's invalid, we return with an error
    if (!directory)
    {
        perror(arg);
        ExitToShell();
    }
    
    errno = 0;
    
    //iterate through the specified directory's contents
    while ( (dentry = readdir(directory)) != NULL)
    {
        //create the file's full path
        strcpy((char *)path, (char *)buf);
        strcat((char *)path, (char *)&"/");
        strcat((char *)path, (char *)&dentry->d_name);
        
        ListItem((char *)path, (char *)&dentry->d_name);
        
        errno = 0;
    }
    
    //report errors and close dir
    if (errno != 0)
        perror("readdir(3)");
    
    if (closedir(directory) == -1)
        perror("closedir(3)");
    
    //dispose of memory allocated for strings
    free(buf);
    free(path);
}


////////////////////////////////////////
// List some item in directory
///////////////////////////////////////

void ListItem (char *path, char *name)
{
    FSRef		fileRef;
    OSErr		err = noErr;
    short		isFldr;
    

    //unless the -a paramter is passed, we don't list hidden .* files
    if (name[0] == '.' && !displayAll)
        return;

    // Get file ref to the file or folder pointed to by the path
    err = FSPathMakeRef(path, &fileRef, NULL);
    
    if (err != noErr)
    {
        printf("FSPathMakeRef(): Error %d returned when getting file reference from %s\n", err, path);
        //ExitToShell();
        return;
    }
    
    // Check if we're dealing with a folder
    isFldr = UnixIsFolder(path);
    if (isFldr == -1)//an error occurred in stat
    {
            perror(path);
            return;
    }
    
    if (isFldr)// it's a folder
    {
        if (omitFolders)
            return;
        else
            ListFolder(name, path, fileRef);
    
    }
    else// regular file
    {
        if (foldersOnly)
            return;
        else
            ListFile(name, path, fileRef);
    }
}


////////////////////////////////////////
// Print directory item info for a file
///////////////////////////////////////

void ListFile (char *name, char *path, FSRef fileRef)
{
    //File manager structures
    FSSpec		fileSpec;
    FInfo 		finderInfo;
    
    char		*fileType;
    char		*creatorType;
    char		*sizeStr;
    char		quote = ' ';
    char		isInvisible, hasCustomIcon, isLocked, hasBundle, isAlias, isStationery;
    char		fflagstr[7];
    char		*fileName;
    
    UInt64		totalPhysicalSize;
    UInt64		totalLogicalSize;
    long		size;
    OSErr		err = noErr;

    //retrive filespec from file ref
    err = FSGetCatalogInfo (&fileRef, NULL, NULL, NULL, &fileSpec, NULL);
    if (err != noErr)
    {
        printf("FSGetCatalogInfo(): Error %d getting file spec from file reference", err);
        ExitToShell();
    }
    
    //get the finder info   
    err = FSpGetFInfo (&fileSpec, &finderInfo);
    if (err != noErr)
    {
        printf("FSpGetFInfo(): Error %d getting file finder info from file spec", err);
        ExitToShell();
    }

    /////// Finder flags/////// 
    
        //Is Invisible
        if (finderInfo.fdFlags & kIsInvisible)
            isInvisible = 'I';
        else
            isInvisible = '-';
        
        //Has Custom Icon
        if (finderInfo.fdFlags & kHasCustomIcon)
            hasCustomIcon = 'C';
        else
            hasCustomIcon = '-';
            
        //Is Locked
        if (finderInfo.fdFlags & kNameLocked)
            isLocked = 'L';
        else
            isLocked = '-';
        
        //Has Bundle Bit Set
        if (finderInfo.fdFlags & kHasBundle)
            hasBundle = 'B';
        else
            hasBundle = '-';
        
        //Is Alias
        if (finderInfo.fdFlags & kIsAlias)
            isAlias = 'A';
        else
            isAlias = '-';
        
        //Is Stationery
        if (finderInfo.fdFlags & kIsStationery)
            isStationery = 'S';
        else
            isStationery = '-';
            
        sprintf((char *)&fflagstr, "%c%c%c%c%c%c ", isInvisible, hasCustomIcon, isLocked, hasBundle, isAlias, isStationery);
    

    /////// File/Creator types /////// 

        //get file type string pointer
        fileType = OSTypeToStr(finderInfo.fdType);
        if (!strlen((char *)fileType))
            strcpy((char *)fileType, (char *)&"    ");
            
        //get creator type string pointer
        creatorType = OSTypeToStr(finderInfo.fdCreator);
        if (!strlen((char *)creatorType))
            strcpy((char *)creatorType, (char *)&"    ");
        
        

    /////// File Sizes /////// 
    err = GetForkSizes(&fileRef,  &totalLogicalSize, &totalPhysicalSize, forkToDisplay);
    if (err != noErr)
    {
        printf("GetForkSizes(): Error %d getting size of file forks\n", err);
        ExitToShell();
    }
    
    //////////////////////////////
    // This needs to be fixed in the future
    // The size of files larger than 4GB is not displayed correctly due
    // due the use of a long int to store file size
    //////////////////////////////
    if (physicalSize)
        size = totalPhysicalSize;
    else
        size = totalLogicalSize;
    
    size = totalLogicalSize;

    switch (displaySize)
    {
	case NORMAL:
	    sizeStr = GetSizeString(size);
	    break;
	case BASE10:
	     sizeStr = GetHumanSizeString(size);
	    break;
	case BASE2:
	    sizeStr = GetSISizeString(size);
    }
    
    /*if (!useBytesForSize)
        sizeStr = GetHumanSizeString(size);
    else
        sizeStr = GetSizeString(size);*/


    ////////// if the -Q option is specified //////////

    if (useQuotes)
        quote = '"';
    
    ////////// if the -p option is specified //////////
    if (printFullPath)
        fileName = path;
    else
        fileName = name;
    
    
    ////////// if the -x option is specified //////////
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
    

    ///////// Print output for this directory item //////////
    
    printf("%s %s %s  %s %c%s%c\n", (char *)&fflagstr, fileType, creatorType, sizeStr, quote, fileName, quote);   

    
    
    //dispose of memory allocated for strings
    free(fileType);
    free(creatorType);
    free(sizeStr);
}

////////////////////////////////////////
// Print directory item info for a folder
///////////////////////////////////////
void ListFolder (char *name, char *path, FSRef fileRef)
{
    char	quote = ' ';
    //char	fflagstr[7] = "------";
    long	valence;
    char	*numFilesStr;
    char	*fileName = name;
    char	humanSizeStr[] = "     -   ";
    char	byteSizeStr[] = "            -  ";
    char	*sizeStr = (char *)&humanSizeStr;

    // Retrive number of files within folder from the 
    // FSCatalog record
    valence = GetNumFilesInFolder(&fileRef);
    if (valence == -1)//error
    {
        printf("%s: Error getting number of files in folder\n", name);
        return;
    }
    
    // generate a suitable-length string from this number
    numFilesStr = GetNumFilesString(valence);
    if (numFilesStr == NULL)
    {
        printf("%s: Error getting number of files in folder\n", name);
        return;
    }
    
    //modify according to the options specified
    if (printFullPath)
        fileName = path;
    
    if (displaySize == BASE2 | displaySize == BASE10)
        sizeStr = (char *)&byteSizeStr;
    
    if (useQuotes)
        quote = '"';
    
    printf("------ %s %s %c%s/%c\n", numFilesStr, sizeStr, quote, fileName, quote);
    
    //dispose of allocated memory
    free(numFilesStr);
}




#pragma mark -

////////////////////////////////////////
// Generate a string for number of files within a folder
///////////////////////////////////////

char* GetNumFilesString (long numFiles)
{
    char	numStr[5] = "\0";
    char	*numFilesStr = NULL;
    short	len,i;
    
    //there can't be less than 0 files in a folder...duh
    if (numFiles < 0)
        return NULL;
    
    //we list a maximum of 9999 files within folder
    //this needs to be fixed at some point
    if (numFiles > 9999)
        numFiles = 9999;
    
    //allocate memory for the string we're going to return
    numFilesStr = malloc(10);
    if (numFilesStr == NULL)
        return NULL;
    
    //create a string that contains just the number
    sprintf((char *)&numStr, "%d", (int)numFiles);
    
    len = strlen((char *)&numStr);
    
    //add spaces as neccesary and then the actual string
    for (i = 0; i < 4 - len; i++)
    {
        strcat((char *)numFilesStr, (char *)&" ");
    }
    strcat((char *)numFilesStr, (char *)&numStr);
    strcat((char *)numFilesStr, (char *)&" items");
    
    return numFilesStr;
}




////////////////////////////////////////
// Get the number of files contained within
// the folder pointed to by an FSRef
///////////////////////////////////////
long GetNumFilesInFolder (FSRef *fileRef)
{
    OSErr		err;
    FSCatalogInfo	catInfo;
    
    //access the FSCatalog record to get the number of files
    err = FSGetCatalogInfo(fileRef, kFSCatInfoValence, &catInfo, NULL, NULL, NULL);

    if (err)
        return(-1);

    return (catInfo.valence);

}

////////////////////////////////////////
// Check parameter for the -f option
// and see if it's valild.  If so, then
// return the setting
///////////////////////////////////////
short GetForkParameterFromString (char *str)
{
    if (strlen(str) != 4)
    {
        printf("Illegal parameter: %s\nYou must specify one of the following: rsrc, data, both", str);
        ExitToShell();
    }
    
    //resource fork specified - 'rsrc'
    if (!strcmp((char *)&RSRC_FORK_PARAM_NAME, (char *)str))
        return DISPLAY_FORK_RSRC;
    
    //data fork specified - 'data'
    if (!strcmp((char *)&DATA_FORK_PARAM_NAME, (char *)str))
        return DISPLAY_FORK_DATA;

    //both forks specified - 'both'
    if (!strcmp((char *)&BOTH_FORK_PARAM_NAME, (char *)str))
        return DISPLAY_FORK_BOTH;

    printf("Illegal parameter: %s\nYou must specify one of the following: rsrc, data, both\n", str);
    ExitToShell();
    
    //to appease the compiler
    return 0;
}


////////////////////////////////////////
// Generate file size string in bytes
///////////////////////////////////////

char* GetSizeString (long size)
{
    char	*sizeStr = NULL;
    char	str[14];
    short	cnt, i;
    
    sizeStr = malloc(14);
    
    sprintf((char *)&str, "%d", (int)size);
    cnt = 12 - strlen((char *)&str);
    
    for(i = 0; i < cnt; i++)
    {
        strcat((char *)sizeStr, (char *)&" ");
    }
    strcat((char *)sizeStr, (char *)&str);
    strcat((char *)sizeStr, (char *)&" B");
    
    return sizeStr;
}

////////////////////////////////////////
// Generate file size string in human readable format
///////////////////////////////////////

char* GetHumanSizeString (long size)
{
    char	*humanSizeStr = NULL;
    char	str[5] = "\0";
    short	cnt,i;
    short	sizeint;
    float	fraction;
    
    humanSizeStr = malloc(10);

    if (size < 1000) //bytes
    {
        sprintf((char *)&str, "%d", (int)size);
        cnt = 5 - strlen((char *)&str);
        
        for(i = 0; i < cnt; i++)
        {
            strcat((char *)humanSizeStr, (char *)&" ");
        }
        
        strcat((char *)humanSizeStr, (char *)&str);
        strcat((char *)humanSizeStr, (char *)&"  B");
    }
    else if (size < 1000000)//kbytes
    {
        sizeint = (size/1000);
    
        sprintf((char *)&str, "%d", sizeint);
        cnt = 5 - strlen((char *)&str);
        
        for(i = 0; i < cnt; i++)
        {
            strcat((char *)humanSizeStr, (char *)&" ");
        }
        
        strcat((char *)humanSizeStr, (char *)&str);
        strcat((char *)humanSizeStr, (char *)&" KB");
    }
    else if (size < 1000000000)//megabytes
    {
        fraction = size;
        fraction /= 1000000;
    
        sprintf((char *)&str, "%3.1f", fraction);
        cnt = 5 - strlen((char *)&str);
        
        for(i = 0; i < cnt; i++)
        {
            strcat((char *)humanSizeStr, (char *)&" ");
        }
        
        strcat((char *)humanSizeStr, (char *)&str);
        strcat((char *)humanSizeStr, (char *)&" MB");
    }
    else//gigabytes
    {
        fraction = size;
        fraction /= 1000000000;
        
        sprintf((char *)&str, "%3.1f", fraction);
        cnt = 5 - strlen((char *)&str);
        
        for(i = 0; i < cnt; i++)
        {
            strcat((char *)humanSizeStr, (char *)&" ");
        }
        
        strcat((char *)humanSizeStr, (char *)&str);
        strcat((char *)humanSizeStr, (char *)&" GB");
    }
    
    return (humanSizeStr);
}

char* GetSISizeString (long size)
{
    char	*humanSizeStr = NULL;
    char	str[5] = "\0";
    short	cnt,i;
    short	sizeint;
    float	fraction;

    humanSizeStr = malloc(10);

    if (size < 1024) //bytes
    {
        sprintf((char *)&str, "%d", (int)size);
        cnt = 5 - strlen((char *)&str);

        for(i = 0; i < cnt; i++)
        {
            strcat((char *)humanSizeStr, (char *)&" ");
        }

        strcat((char *)humanSizeStr, (char *)&str);
        strcat((char *)humanSizeStr, (char *)&"  B");
    }
    else if (size < 1048576)//kibibytes
    {
        sizeint = (size/1024);

        sprintf((char *)&str, "%d", sizeint);
        cnt = 5 - strlen((char *)&str);

        for(i = 0; i < cnt; i++)
        {
            strcat((char *)humanSizeStr, (char *)&" ");
        }

        strcat((char *)humanSizeStr, (char *)&str);
        strcat((char *)humanSizeStr, (char *)&" KiB");
    }
    else if (size < 1073741824)//mebibytes
    {
        fraction = size;
        fraction /= 1048576;

        sprintf((char *)&str, "%3.1f", fraction);
        cnt = 5 - strlen((char *)&str);

        for(i = 0; i < cnt; i++)
        {
            strcat((char *)humanSizeStr, (char *)&" ");
        }

        strcat((char *)humanSizeStr, (char *)&str);
        strcat((char *)humanSizeStr, (char *)&" MiB");
    }
    else//gibibytes
    {
        fraction = size;
        fraction /= 1073741824;

        sprintf((char *)&str, "%3.1f", fraction);
        cnt = 5 - strlen((char *)&str);

        for(i = 0; i < cnt; i++)
        {
            strcat((char *)humanSizeStr, (char *)&" ");
        }

        strcat((char *)humanSizeStr, (char *)&str);
        strcat((char *)humanSizeStr, (char *)&" GiB");
    }

    return (humanSizeStr);
}



////////////////////////////////////////////
// Function to get size of file forks
////////////////////////////////////////////

OSErr GetForkSizes (const FSRef *fileRef,  UInt64 *totalLogicalForkSize, UInt64 *totalPhysicalForkSize, short fork)  
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


    // if logcal fork size is needed
    if (totalLogicalForkSize != NULL)
    {
        *totalLogicalForkSize = 0;
        forkLogicalSizePtr = &forkLogicalSize;
    }
    else
    {
        forkLogicalSizePtr = NULL;
    }
    
    // if physical fork size is needed
    if (totalPhysicalForkSize != NULL)
    {
        *totalPhysicalForkSize = 0;
        forkPhysicalSizePtr = &forkPhysicalSize;
    }
    else
    {
        forkPhysicalSizePtr = NULL;
    }
    
    // Iterate through the file's forks and get their combined size
    forkIterator.initialize = 0;
    
    do
    {
        err = FSIterateForks(fileRef, &forkIterator, &forkName, forkLogicalSizePtr, forkPhysicalSizePtr);
        if (noErr == err)
        {
            HFSUniPStrToCString(&forkName, (char *)&forkStr);
            
            //if it's the resource fork we're doing now
            if (!strcmp((char *)&"RESOURCE_FORK", (char *)&forkStr))
            {
                //if we're not just displaying the data fork
                if (fork != DISPLAY_FORK_DATA)
                {
                    if (totalLogicalForkSize != NULL)
                        *totalLogicalForkSize += forkLogicalSize;
                    if (totalPhysicalForkSize != NULL)
                        *totalPhysicalForkSize += forkPhysicalSize;
                }
            }
            else//must be the data fork, then
            {
                if (fork != DISPLAY_FORK_RSRC)
                {
                    if (totalLogicalForkSize != NULL)
                        *totalLogicalForkSize += forkLogicalSize;
                    if (totalPhysicalForkSize != NULL)
                        *totalPhysicalForkSize += forkPhysicalSize;
                }
            }
        }
        
    } while (err == noErr);
    
    
    // errFSNoMoreItems is not serious, we report other errors
    if (err && err != errFSNoMoreItems)
        return err;
    else
        err = noErr;
    
    return(err);
}

////////////////////////////////////////
// Cut a file suffix of up to 4 characters
// from listed file names and return it
// It is then listed in a seperate column
// because of the -x option
///////////////////////////////////////

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


////////////////////////////////////////
// Transform OSType into a C string
// An OSType is just a 4 character string
// stored as a long integer
///////////////////////////////////////
char* OSTypeToStr(OSType aType)
{
    char *t = malloc(5);
    BlockMoveData((void *)&aType, t, 4);
    t[4] = 0;
    
    return t;
}



////////////////////////////////////////
// Check if FSRef is a folder
///////////////////////////////////////
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

////////////////////////////////////////
// Check if file in designated path is folder
// This is faster than the File-Manager based
// function above
///////////////////////////////////////
short UnixIsFolder (char *path)
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

////////////////////////////////////////
// HFSUniStr255 converted to null-terminated C string
// For some inexplicable reason, there seems to be no
// convenient Apple function to do this without
// first converting to a Core Foundation string
///////////////////////////////////////
void HFSUniPStrToCString (HFSUniStr255 *uniStr, char *cstr)
{
    CFStringRef		cfStr;

    cfStr = CFStringCreateWithCharacters(kCFAllocatorDefault,uniStr->unicode,uniStr->length);
    
    CFStringGetCString(cfStr, cstr, 255, kCFStringEncodingUTF8);
}
