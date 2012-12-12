/*
    fileinfo - list info about files in MacOS X
    Copyright (C) 2003 	Sveinbjorn Thordarson <sveinbt@hi.is>

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

	ATTENTION!
	
	This code is very buggy and incomplete....my apologies :)
*/

/*
	Version History

	Version 0.1 - fileinfo released despite bugs, flaws, shortcomings, etc
*/

/*  Todo

	* Clean up the messy code
	* Fix a lot of loose ends
	* Serious error checking
	* Finder comment retrieval
	* Bugs, bugs, bugs

*/


#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <Carbon/Carbon.h>
#include <string.h>


#define		PROGRAM_STRING  	"fileinfo"
#define		VERSION_STRING		"0.1"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson <sveinbt@hi.is>"

#define		MAX_PATH_LENGTH		1024
#define		MAX_FILENAME_LENGTH	256

#define		FILETYPE_FILE		0
#define		FILETYPE_FOLDER		1
#define		FILETYPE_ALIAS		2
#define		FILETYPE_SYMLINK	3
#define		FILETYPE_CHARDEV	4
#define		FILETYPE_BLOCKDEV   5
#define		FILETYPE_PIPE		6
#define		FILETYPE_SOCKET		7
#define		FILETYPE_WHITEOUT	8
#define		FILETYPE_UNKNOWN	9


#define		DISPLAY_FORK_BOTH	0
#define		DISPLAY_FORK_DATA	1
#define		DISPLAY_FORK_RSRC	2

#define		SIZE_BYTES				0
#define		SIZE_HUMAN				1
#define		SIZE_HUMAN_SI			2

#define		OPT_STRING		"vh"

const char  fileTypeStrings[7][32] = { "File", "Folder", "Alias", "Symbolic Link", "Character Device", "Block Device", "Named Pipe (FIFO)", "UNIX Socket", "Whiteout", "Uknown File Type" };
const char  labelNames[8][8] = { "None", "Red", "Orange", "Yellow", "Green", "Blue", "Purple", "Gray" };
const char  finderFlagNames[6][32] = { "Invisible", "CustomIcon", "NameLocked", "BundleBit", "Alias", "Stationery" };


typedef struct
{
	char		path[1024];

	char		name[256];
	char		targetPath[1024];
	
	short		kind;
	
	FSSpec		fsSpec;
	FSRef		fsRef;
	FInfo		finderInfo;
	FXInfo		finderXInfo;
	
	UInt64		rsrcPhysicalSize;
	UInt64		dataPhysicalSize;
	UInt64		rsrcLogicalSize;
	UInt64		dataLogicalSize;
	UInt64		totalLogicalSize;
	UInt64		totalPhysicalSize;
	
	char		fileType[5];
	char		fileCreator[5];
	
	Boolean		finderFlags[6];
	short		labelNum;
	
	char				dateCreatedString[256];
	char				dateModifiedString[256];
	char				dateAccessedString[256];
	char				dateAttrModString[256];
	
	char		ownerPermissions[3];
	char		groupPermissions[3];
	char		otherPermissions[3];

} FileInfoStruct;


static void PrintVersion (void);
static void PrintHelp (void);
void PrintFileInfo (char *path);
static int UnixIsFolder (char *path);
static OSErr GetForkSizes (const FSRef *fileRef,  UInt64 *totalLogicalForkSize, UInt64 *totalPhysicalForkSize, short fork);


/*//////////////////////////////////////
// Main program function
/////////////////////////////////////*/
int main (int argc, char *argv[]) 
{
    int				i;
    int				rc;
    int				optch;
    static char		optstring[] = OPT_STRING;

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
			default: /* '?' */
                rc = 1;
                PrintHelp();
                return 0;
        }
    }

	
	for (; optind < argc; ++optind)
		PrintFileInfo( argv[optind] );
	
	//PrintFileInfo("/Users/sveinbjorn/Desktop/Untitled.txt");
    return(0);
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
    printf("usage: %s [-%s] file ...\n", PROGRAM_STRING, OPT_STRING);
}


#pragma mark -



void PrintFileInfo (char *path)
{
	short				i = 0;
	char				buf[1024];
	char				finderFlagsString[512] = "\0";
	FileInfoStruct		file;
	OSErr				err = noErr;
	
	strcpy(file.path, path);
	err = RetrieveStatData(&file);
	if (err)
	{
		(void)fprintf(stderr, "\nfileinfo: %s: Error %d when retrieving stat data\n", &file.path, err);
		return;
	}
	err = RetrieveFileInfo (&file);
	if (err)
	{
		(void)fprintf(stderr, "\nfileinfo: %s: Error %d when retrieving file info\n", &file.path, err);
		return;
	}
	
	//Generate flag string
	for (i = 0; i < 6; i++)
	{
		if (file.finderFlags[i] == 1)
			strcat(&finderFlagsString, &finderFlagNames[i]);
			strcat(&finderFlagsString, &" ");
	}
	if (!strlen(&finderFlagsString))
		strcpy(&finderFlagsString, &"None");
	
	strcpy(&file.name, GetFileNameFromPath(file.path));
	
	printf("     Name: \"%s\"\n", file.name);
	printf("     Path: \"%s\"\n", file.path);
	if (file.kind == FILETYPE_SYMLINK || file.kind == FILETYPE_ALIAS)
		printf("     Kind:  %s --> \"%s\"\n", &fileTypeStrings[file.kind], file.targetPath);
	else
		printf("     Kind:  %s\n", &fileTypeStrings[file.kind]);
	printf("     Size:  %s (%llu bytes)\n", GetSizeString(file.totalPhysicalSize, SIZE_HUMAN), file.totalLogicalSize);
	printf("    Forks:  Data (%llu bytes), Resource (%llu bytes)\n\n", file.dataLogicalSize,  file.rsrcLogicalSize);
	
	printf("     Type: \"%s\"\n", file.fileType);
	printf("  Creator: \"%s\"\n", file.fileCreator);
	printf("    Label:  %s\n", &labelNames[file.labelNum]);
	printf("    Flags:  %s\n\n", &finderFlagsString);
	
	printf("  Created:  %s\n", &file.dateCreatedString);
	printf(" Modified:  %s\n", &file.dateModifiedString);
	printf(" Accessed:  %s\n", &file.dateAccessedString);
	printf("Attr. Mod:  %s\n\n", &file.dateAttrModString);
	
	printf("           Read Write Exec\n");
	printf("    Owner:  [%c]  [%c]  [%c]\n", file.ownerPermissions[0], file.ownerPermissions[1], file.ownerPermissions[2]);
	printf("    Group:  [%c]  [%c]  [%c]\n", file.groupPermissions[0], file.groupPermissions[1], file.groupPermissions[2]);
	printf("   Others:  [%c]  [%c]  [%c]\n", file.otherPermissions[0], file.otherPermissions[1], file.otherPermissions[2]);
	//printf("\n%d\n", cinfo.permissions);

}


OSErr RetrieveStatData (FileInfoStruct *file)
{
	struct  stat filestat;
	short   err;
	int		lnklen;
	char	buf[20];
    
    err = lstat(file->path, &filestat);
    if (err == -1)
        return err;
		
	strmode(filestat.st_mode, &buf);
	
	switch(buf[0])
	{
		//regular file
		case '-':
			file->kind = FILETYPE_FILE;
			break;
		case 'b':
			file->kind = FILETYPE_BLOCKDEV;
			break;
		case 'c':
			file->kind = FILETYPE_CHARDEV;
			break;
		case 'd':
			file->kind = FILETYPE_FOLDER;
			break;
		case 'l':
			file->kind = FILETYPE_SYMLINK;
			if ((lnklen = readlink(&file->path, file->targetPath, sizeof(file->targetPath) - 1)) == -1) 
			{
				(void)fprintf(stderr, "\nfileinfo: %s: %s\n", &file->path, strerror(errno));
				return errno;
			}
			file->path[lnklen] = '\0';
			break;
		case 'p':
			file->kind = FILETYPE_PIPE;
			break;
		case 's':
			file->kind = FILETYPE_SOCKET;
			break;
		case 'w':
			file->kind = FILETYPE_WHITEOUT;
			break;
		case '?':
			file->kind = FILETYPE_UNKNOWN;
			break;
	}
	
	file->ownerPermissions[0] = (buf[1] == 'r') ? '*' : ' ';
	file->ownerPermissions[1] = (buf[2] == 'w') ? '*' : ' ';
	file->ownerPermissions[2] = (buf[3] == 'x') ? '*' : ' ';
	
	file->groupPermissions[0] = (buf[4] == 'r') ? '*' : ' ';
	file->groupPermissions[1] = (buf[5] == 'w') ? '*' : ' ';
	file->groupPermissions[2] = (buf[6] == 'x') ? '*' : ' ';
	
	file->otherPermissions[0] = (buf[7] == 'r') ? '*' : ' ';
	file->otherPermissions[1] = (buf[8] == 'w') ? '*' : ' ';
	file->otherPermissions[2] = (buf[9] == 'x') ? '*' : ' ';

	return noErr;
}


OSErr RetrieveFileInfo (FileInfoStruct *file)
{
	FSRef				fileRef;
	FSSpec				fileSpec;
	OSErr				err = noErr;
	FSCatalogInfoBitmap cinfoMap = kFSCatInfoPermissions + kFSCatInfoFinderXInfo + kFSCatInfoCreateDate + kFSCatInfoContentMod 
								 + kFSCatInfoAttrMod + kFSCatInfoAccessDate + kFSCatInfoDataSizes + kFSCatInfoRsrcSizes + kFSCatInfoFinderInfo;
	FSCatalogInfo		cinfo;

	/* Get file ref to the file or folder pointed to by the path */
    err = FSPathMakeRef(&file->path, &file->fsRef, NULL);
	if (err != noErr) 
    {
        printf("FSPathMakeRef(): Error %d returned when getting file reference from %s\n", err, file->path);
        /* ExitToShell(); */
        return err;
    }
	

	//Retrieve File System Catalog information from an FSRef
	err = FSGetCatalogInfo (&file->fsRef, cinfoMap, &cinfo, NULL, &file->fsSpec, NULL);
	if (err != noErr) 
    {
        printf("FSGetCatalogInfo(): Error %d returned when retrieving catalog information from %s\n", err, file->path);
        /* ExitToShell(); */
        return err;
    }
	
		//finder info
		BlockMove(&cinfo.finderInfo, &file->finderInfo, sizeof(FInfo));
		BlockMove(&cinfo.extFinderInfo, &file->finderXInfo, sizeof(FXInfo));
		
		//file size
		file->rsrcPhysicalSize = cinfo.rsrcPhysicalSize;
		file->dataPhysicalSize = cinfo.dataPhysicalSize;
		file->rsrcLogicalSize = cinfo.rsrcLogicalSize;
		file->dataLogicalSize = cinfo.dataLogicalSize;
		file->totalLogicalSize = file->rsrcLogicalSize + file->dataLogicalSize;
		file->totalPhysicalSize = file->rsrcPhysicalSize + file->dataPhysicalSize;
		
		//dates
		GetDateTimeStringFromUTCDateTime(&cinfo.createDate, &file->dateCreatedString);
		GetDateTimeStringFromUTCDateTime(&cinfo.contentModDate, &file->dateModifiedString);
		GetDateTimeStringFromUTCDateTime(&cinfo.accessDate, &file->dateAccessedString);
		GetDateTimeStringFromUTCDateTime(&cinfo.attributeModDate, &file->dateAttrModString);
	
	err = ProcessFinderInfo(file);
	if (err) { return err; }
	
	
	return noErr;
	
}

OSErr ProcessFinderInfo (FileInfoStruct *file)
{
	//get file label
	file->labelNum = GetLabelNumber(file->finderInfo.fdFlags);

	/* get file type string */
	OSTypeToStr(file->finderInfo.fdType, file->fileType);
	
	/* get creator type string */
	OSTypeToStr(file->finderInfo.fdCreator, file->fileCreator);
	
	/* ///// Finder flags////// */
    
	/* Is Invisible */
	file->finderFlags[0] = (file->finderInfo.fdFlags & kIsInvisible) ? 1 : 0;

	/* Has Custom Icon */
	file->finderFlags[1] = (file->finderInfo.fdFlags & kHasCustomIcon) ? 1 : 0;

	/* Is Locked */
	file->finderFlags[2] = (file->finderInfo.fdFlags & kNameLocked) ? 1 : 0;

	/* Has Bundle Bit Set */
	file->finderFlags[3] = (file->finderInfo.fdFlags & kHasBundle) ? 1 : 0;

	/* Is Alias */
	file->finderFlags[4] = (file->finderInfo.fdFlags & kIsAlias) ? 1 : 0;

	/* Is Stationery */
	file->finderFlags[5] = (file->finderInfo.fdFlags & kIsStationery) ? 1 : 0;

}


#pragma mark -

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

static int UnixIsSymlink (char *path)
{
    struct stat filestat;
    short err;
    
    err = stat(path, &filestat);
    if (err == -1)
        return err;

    return (S_ISLNK(filestat.st_mode) != 1);
}

////////////////////////////////////////
// Retrieves pointer to name of file from path
///////////////////////////////////////

char* GetFileNameFromPath (char *name)
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


/*//////////////////////////////////////
// On being passed the path to a MacOS alias,
// returns path to the original file to which
// it refers.  Returns null if file not found.
/////////////////////////////////////*/
static char* GetPathOfAliasSource (char *path)
{
    OSErr	err = noErr;
    static char	srcPath[1024];
    FSRef	fileRef;
    Boolean	isAlias, isFolder;

    //get file reference from path given
    err = FSPathMakeRef (path, &fileRef, NULL);
    if (err != noErr)
    {
        printf("Error getting file spec from path.\n");
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
 
    result = FSRefMakePath(&fileRef, path, 1024);

    return ( result );
}

/*
//////////////////////////////////////
// Generate file size string
//////////////////////////////////////
*/
static char* GetSizeString( UInt64 size, short sizeFormat)
{
	static char	sizeStr[15];

	switch( sizeFormat ) {
		default:
		case SIZE_BYTES:
			snprintf( sizeStr, sizeof(sizeStr), "%14llu", size );
			break;
		case SIZE_HUMAN:
			if( size < 1024ULL ) {
				/* bytes */
				snprintf( sizeStr, sizeof(sizeStr), "%6u  B", (unsigned int)size );
			} else if( size < 1048576ULL) {
				/* kbytes */
				snprintf( sizeStr, sizeof(sizeStr), "%5.1f KB", size / 1024.0 );
			} else if( size < 1073741824ULL ) {
				/* megabytes */
				snprintf( sizeStr, sizeof(sizeStr), "%5.1f MB", size / 1048576.0 );
			} else {
				/* gigabytes */
				snprintf( sizeStr, sizeof(sizeStr), "%5.1f GB", size / 1073741824.0 );
			}
			break;
		case SIZE_HUMAN_SI:
			if( size < 1000ULL ) {
				/* bytes */
				snprintf( sizeStr, sizeof(sizeStr), "%6u  B", (unsigned int)size );
			} else if( size < 1000000ULL) {
				/* kbytes */
				snprintf( sizeStr, sizeof(sizeStr), "%5.1f KB", size / 1000.0 );
			} else if( size < 1000000000ULL ) {
				/* megabytes */
				snprintf( sizeStr, sizeof(sizeStr), "%5.1f MB", size / 1000000.0 );
			} else {
				/* gigabytes */
				snprintf( sizeStr, sizeof(sizeStr), "%5.1f GB", size / 1000000000.0 );
			}
			break;
	}

	return( sizeStr );
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


OSErr GetDateTimeStringFromUTCDateTime (UTCDateTime *utcDateTime, char *dateTimeString)
{
	CFAbsoluteTime  cfTime;
	LongDateTime	ldTime;
	Str255			pDateStr, pTimeStr;
	char			dateStr[128], timeStr[128];
	OSErr			err = noErr;

	err = UCConvertUTCDateTimeToCFAbsoluteTime (utcDateTime, &cfTime);
	err = UCConvertCFAbsoluteTimeToLongDateTime (cfTime, &ldTime);
	
	LongDateString (&ldTime, shortDate, pDateStr, NULL);
	LongTimeString (&ldTime, TRUE, pTimeStr, NULL);
	
	CopyPascalStringToC (pDateStr, &dateStr);
	CopyPascalStringToC (pTimeStr, &timeStr);
	
	strcpy(dateTimeString, &timeStr);
	strcat(dateTimeString, &" ");
	strcat(dateTimeString, &dateStr);
	
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
