/*
 lsmac - ls-like utility for viewing MacOS file meta-data
 Copyright (C) 2003 Sveinbjorn Thordarson, Ingmar J. Stein

    Authors: 	Sveinbjorn Thordarson	 	<sveinbt@hi.is>
                Ingmar J. Stein 		<stein@xtramind.com>

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
0.4
* made internal methods static (reduced file size by more than 50%)
* optimized dynamic memory allocation and other performance enhancements
* Define KB, MB and GB as 2^10, 2^20 and 2^30 bytes, respectively
* List multiple directories given as command line arguments
* Made lsmac accept files as command line parameter, and then just print the info about those files
* Displays size correctly for files larger than 4GB or so.
* added -A, -R, -U, -h and -r options, removed -p, -o and -F options
* Prefer POSIX API over Carbon API (use exit (3) instead of ExitToShell)
* use buffer safe snprintf
* handle block devices and symlinks
* 9999 files per folder limit removed
* added sizes for directories, links, etc.

0.3
* lsmac can now be told whether to list the size of resource fork, data fork or both.  Both is default
* -Q option: names/paths printed within quotation marks
* lsmac now displays file size in human readable format by default
* fixed error that occurred when listing root directory
* -F option: only folders are listed
* Num. of files within folders is now listed by default
* Some minor optimizations (f.e. stat used for determining directories, free & malloc instead of NewPtr & DisposePtr, etc.)
* Dynamic memory allocation for


0.2 Tons of stuff changed/improved
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

* Try to optimize speed.  lsmac default output is 2x-3x slower than ls -l
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

typedef struct fileInfo_s 
{
	char		*name;
	char		*path;
	struct stat	lstat;
} fileInfo_t;

static void PrintVersion( void );
static void PrintHelp( void );

static void ListDirectoryContents( const char *arg );
static void ListItem( const fileInfo_t *item );
static void ListFile( const fileInfo_t *item );

static long GetNumFilesInFolder( const char *folder );

static int GetForkParameterFromString( const char *str );

static char* GetSizeString( UInt64 size );
static OSErr GetForkSizes( const FSRef *fileRef,  UInt64 *totalLogicalForkSize, UInt64 *totalPhysicalForkSize, int fork );

static void OSTypeToStr(OSType aType, char *aStr);
static void HFSUniPStrToCString (HFSUniStr255 *uniStr, char *cstr);

/*///////Definitions///////////////////*/

#define		PROGRAM_STRING  	"lsmac"
#define		VERSION_STRING		"0.4"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson, Ingmar J. Stein"

#define		MAX_PATH_LENGTH		4096
#define		MAX_FILENAME_LENGTH	256

#define		OPT_STRING			"vhf:FsboalQrRAU"

#define		DISPLAY_FORK_BOTH	0
#define		DISPLAY_FORK_DATA	1
#define		DISPLAY_FORK_RSRC	2

#define		BOTH_FORK_PARAM_NAME	"both"
#define		DATA_FORK_PARAM_NAME	"data"
#define		RSRC_FORK_PARAM_NAME	"rsrc"

#define		SIZE_BYTES				0
#define		SIZE_HUMAN				1
#define		SIZE_HUMAN_SI			2

#define		SORT_NONE				0
#define		SORT_NAME				1

/*///////Globals///////////////////*/

static int		displayHidden = false;
static int		displayImplied = false;
static int		sizeFormat = SIZE_BYTES;
static int		physicalSize = false;
static int		forkToDisplay = DISPLAY_FORK_BOTH;
static int		useQuotes = false;
/* static int		listSuffix = false; */
static int		recursive = false;
static int		reverse = false;
static int		sort = SORT_NAME;

/*
 static int		calcFolderSizes = false;
 static int		calcNumFolderItems = false;
 */

/*
 Command line options

 v - version
 h - print sizes in human readable format (e.g., 1K 234M, 2G)
 H - likewise, but use powers of 1000 not 1024
 s - display size
 b - display size in bytes
 o - omit folders - will not omit folder aliases
 a - display all (including hidden . files)
 A - display almost all
 l - when printing size, print physical size, not logical size
 U - do not sort; list entries in directory order
 R - list recursive
 r - reverse sorting

 [-f fork] - select which fork to print size of

 c - calculate folder sizes 				** NOT IMPLEMENTED YET **
 i - calculate number of files within folders 	** NOT IMPLEMENTED YET **

 */

/*
//////////////////////////////////////
// Main program function
//////////////////////////////////////
*/
int main (int argc, char *argv[])
{
	int			i;
	int			optch;
	fileInfo_t	fileinfo;

	while ( (optch = getopt(argc, argv, OPT_STRING)) != -1 ) {
		switch(optch) {
			case 'v':
				PrintVersion();
				return( EXIT_SUCCESS );
				break;
			case 'h':
				sizeFormat = SIZE_HUMAN;
				break;
			case 'H':
				sizeFormat = SIZE_HUMAN_SI;
				break;
			case 'f':
				forkToDisplay = GetForkParameterFromString( optarg );
				if( forkToDisplay == -1 ) {
					return( EXIT_FAILURE );
				}
				break;
			/* not quite ready for this yet */
			/* case 'x':
				listSuffix = true;
				break;*/
			case 'a':
				displayHidden = true;
				displayImplied = true;
				break;
			case 'A':
				displayHidden = true;
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
			case 'r':
				reverse = true;
				break;
			case 'R':
				recursive = true;
				break;
			case 'U':
				sort = SORT_NONE;
				break;
			default: /* '?' */
				PrintHelp();
				return( EXIT_FAILURE );
		}
	}

	argc -= optind;
	argv += optind;

	if( argc ) {
		for( i=0; i<argc; i++ ) {
			if( argc > 1 ) {
				if( i > 0 ) {
					printf("\n");
				}
				printf( "%s:\n", argv[i] );
			}

			if( lstat( argv[i], &fileinfo.lstat ) == -1 ) {
				perror( argv[i] );
			} else {
				if( S_ISDIR( fileinfo.lstat.st_mode ) ) {
					ListDirectoryContents( argv[i] );
				} else {
					fileinfo.name = argv[i];
					fileinfo.path = (char *)malloc( strlen( argv[i] ) + 3 );
					strcpy( fileinfo.path, "./" );
					strcat( fileinfo.path, argv[i] );

					ListFile( &fileinfo );

					free( fileinfo.path );
				}
			}
		}
	} else {
		/* If no path is specified as argument, we use the current working directory */
		ListDirectoryContents( "." );
	}

	return( EXIT_SUCCESS );
}

/*
//////////////////////////////////////
// Print version and author to stdout
/////////////////////////////////////
*/
static void PrintVersion (void)
{
	printf("%s version %s by %s\n", PROGRAM_STRING, VERSION_STRING, AUTHOR_STRING);
}

/*
//////////////////////////////////////
// Print help string to stdout
/////////////////////////////////////
*/
static void PrintHelp( void )
{
	/*printf("%s version %s - ls-like program for viewing Mac meta-data\n", PROGRAM_STRING, VERSION_STRING);*/
	printf("usage: lsmac [-%s] directory\n", OPT_STRING);
}

static int sort_name( const void *e1, const void *e2 )
{
	const fileInfo_t *f1 = (const fileInfo_t *)e1;
	const fileInfo_t *f2 = (const fileInfo_t *)e2;

	return( reverse ? strcmp( f2->name, f1->name ) : strcmp( f1->name, f2->name ) );
}

/*
//////////////////////////////////////
// List some item in directory
/////////////////////////////////////
*/
static void ListItem( const fileInfo_t *item )
{
	/* unless the -a parameter is passed, we don't list hidden .* files */
	if( (!strcmp( item->name, "." ) || !strcmp( item->name, ".." )) && !displayImplied ) 
        {
		return;
	}
	if( item->name[0] == '.' && !displayHidden ) 
        {
		return;
	}

	ListFile( item );
}

/*
//////////////////////////////////////
// Iterate through directory and list its items
//////////////////////////////////////
*/
static void ListDirectoryContents( const char *path )
{
	DIR				*directory;
	struct dirent			*dentry;
	fileInfo_t			*entry;
	fileInfo_t			*entries;
	int				count;
	int				i;
	int				pathLen;

	pathLen = strlen( path );

	if( !pathLen ) 
        {
		fputs("Invalid directory parameter.\n", stderr);
		exit( EXIT_FAILURE );
	}

	if( recursive ) 
        {
		printf( "%s:\n", path );
	}

	count = GetNumFilesInFolder( path );
	if( count == -1 ) 
        {
		return;
	}

	entries = (fileInfo_t *)malloc( count*sizeof(fileInfo_t) );
	if( !entries ) 
        {
		return;
	}

	/* open directory */
	directory = opendir( path );

	/* if it's invalid, we return with an error */
	if( !directory ) 
        {
		free( entries );
		perror( path );
		exit( EXIT_FAILURE );
	}

	/* iterate through the specified directory's contents */
	i = 0;
	while(( dentry = readdir(directory) )) 
        {
		entry = &entries[i];
		entry->name = strdup( dentry->d_name );
		entry->path = (char *)malloc( pathLen + strlen( dentry->d_name ) + 1 );
		sprintf( entry->path, "%s/%s", path, dentry->d_name );
		if( lstat( entry->path, &entry->lstat ) == -1 ) 
                {
			perror( entry->path );
		}
		++i;
	}

	/* close dir */
	if( closedir( directory ) == -1 ) 
        {
		perror( "closedir(3)" );
	}

	/* sort entries */
	switch( sort ) 
        {
		case SORT_NONE:
			break;
		default:
		case SORT_NAME:
			qsort( entries, count, sizeof(fileInfo_t), sort_name );
			break;
	}

	/* print entries */
	for( i=0; i<count; i++ )
        {
		ListItem( &entries[i] );
	}

	/* recurse into subdirectories */
	if( recursive ) 
        {
		for( i=0; i<count; i++ ) 
                {
			entry = &entries[i];
			if( strcmp( entry->name, "." ) && strcmp( entry->name, ".." ) && S_ISDIR(entry->lstat.st_mode) ) 
                        {
				printf( "\n" );
				ListDirectoryContents( entry->path );
			}
		}
	}

	/* free entries */
	for( i=0; i<count; i++ )
        {
		entry = &entries[i];
		free( entry->name );
		free( entry->path );
	}
	free( entries );
}

static long GetNumFilesInFolder( const char *folder )
{
	long	num;
	DIR		*dir;

	num = 0;
	dir = opendir( folder );
	if( !dir ) 
        {
		perror( folder );
		return( -1 );
	}

	while( readdir( dir ) ) 
        {
		num++;
	}

	if( closedir( dir ) == -1 ) 
        {
		perror( folder );
		return( -1 );
	}

	return( num );
}

/*
//////////////////////////////////////
// Generate file size string
//////////////////////////////////////
*/
static char* GetSizeString( UInt64 size )
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

/*
//////////////////////////////////////
// Print directory item info for a file
/////////////////////////////////////
*/
static void ListFile( const fileInfo_t *item )
{
	/* File manager structures */
	FSRef		fileRef;
	FSSpec		fileSpec;
	FInfo 		finderInfo;

	char		fileType[5];
	char		creatorType[5];
	char		*sizeStr;
	const char	*quote;
	char		fflagstr[7];
	long		numFiles;
	int			count;
	char		target[MAX_PATH_LENGTH + MAX_FILENAME_LENGTH + 1];

	UInt64		totalPhysicalSize;
	UInt64		totalLogicalSize;
	UInt64		size;
	OSErr		err = noErr;

	int			isReg;
	int			isDir;
	int			isLnk;

	isReg = S_ISREG(item->lstat.st_mode);
	isDir = S_ISDIR(item->lstat.st_mode);
	isLnk = S_ISLNK(item->lstat.st_mode);

	if( isReg ) 
        {
		/* Get file ref to the file pointed to by the path */
		err = FSPathMakeRef( (unsigned char *)item->path, &fileRef, NULL );

		if( err != noErr ) 
                {
			printf( "FSPathMakeRef(): Error %d returned when getting file reference from %s\n", err, item->name );
			/* exit(); */
			return;
		}

		/* retrieve filespec from file ref */
		err = FSGetCatalogInfo( &fileRef, NULL, NULL, NULL, &fileSpec, NULL );
		if (err != noErr) 
                {
			printf( "FSGetCatalogInfo(): Error %d getting file spec from file reference of file %s\n", err, item->name );
			exit( EXIT_FAILURE );
		}

		/* get the finder info */
		err = FSpGetFInfo( &fileSpec, &finderInfo );
		if( err != noErr ) 
                {
			printf( "FSpGetFInfo(): Error %d getting file finder info from file spec of file %s\n", err, item->name );
			exit( EXIT_FAILURE );
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
	} 
        else 
        {
		fflagstr[0] = '-';
		fflagstr[1] = '-';
		fflagstr[2] = '-';
		fflagstr[3] = '-';
		fflagstr[4] = '-';
		fflagstr[5] = '-';
		fflagstr[6] = '\0';
	}

	if( isDir ) 
        {
		numFiles = GetNumFilesInFolder( item->path );
		if( numFiles == -1 ) 
                {
			/* error */
			return;
		}
	} 
        else 
        {
		numFiles = 1;
	}

	/* ///// File/Creator types ///// */

	if( isReg ) 
        {
		/* get file type string */
		OSTypeToStr( finderInfo.fdType, fileType );

		/* get creator type string */
		OSTypeToStr( finderInfo.fdCreator, creatorType );
	} 
        else 
        {
		fileType[0] = '\0';
		creatorType[0] = '\0';
	}

	/* ///// File Sizes ////// */
	if( isReg ) 
        {
		err = GetForkSizes( &fileRef,  &totalLogicalSize, &totalPhysicalSize, forkToDisplay );
		if( err != noErr ) 
                {
			printf( "GetForkSizes(): Error %d getting size of file forks of file %s\n", err, item->path );
			exit( EXIT_FAILURE );
		}

		size = physicalSize ? totalPhysicalSize : totalLogicalSize;
	}
        else
        {
		size = item->lstat.st_size;
	}
		
	sizeStr = GetSizeString( size );

	/* if the -Q option is specified */
	quote = useQuotes ? "\"" : "";

	/* /////// Print output for this directory item //////// */
	if( isLnk ) 
        {
		count = readlink( item->path, target, sizeof(target) );
		if( count == -1 ) 
                {
			perror( item->path );
			return;
		}

		if( count == sizeof(target) )
                {
			target[count-1] = '\0';
		} 
                else 
                {
			target[count] = '\0';
		}

		printf( "%s %4s %4s  %s %s%s%s -> %s%s%s\n", fflagstr, fileType, creatorType, sizeStr, quote, item->name, quote, quote, target, quote );
	} 
        else 
        {
		printf( "%s %4s %4s  %s %s%s%s\n", fflagstr, fileType, creatorType, sizeStr, quote, item->name, quote );
	}
}

/*
//////////////////////////////////////
// Check parameter for the -f option
// and see if it's valild.  If so, then
// return the setting
//////////////////////////////////////
*/
static int GetForkParameterFromString( const char *str )
{
	if( !strcmp( RSRC_FORK_PARAM_NAME, str ) ) {
		/* resource fork specified - 'rsrc' */
		return( DISPLAY_FORK_RSRC );
	} else if( !strcmp( DATA_FORK_PARAM_NAME, str ) ) {
		/* data fork specified - 'data' */
		return( DISPLAY_FORK_DATA );
	} else if( !strcmp( BOTH_FORK_PARAM_NAME, str ) ) {
		/* both forks specified - 'both' */
		return( DISPLAY_FORK_BOTH );
	} else {
		printf("Illegal parameter: %s\nYou must specify one of the following: rsrc, data, both\n", str);
		return( -1 );
	}
}

/*
//////////////////////////////////////////
// Function to get size of file forks
//////////////////////////////////////////
*/
static OSErr GetForkSizes (const FSRef *fileRef,  UInt64 *totalLogicalForkSize, UInt64 *totalPhysicalForkSize, int fork)
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
	char			forkStr[255];

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
			HFSUniPStrToCString( &forkName, (char *)&forkStr );

			/* if it's the resource fork we're doing now */
			if (!strcmp("RESOURCE_FORK", forkStr)) {
				/* if we're not just displaying the data fork */
				if (fork != DISPLAY_FORK_DATA) {
					if (totalLogicalForkSize) {
						*totalLogicalForkSize += forkLogicalSize;
					}
					if (totalPhysicalForkSize) {
						*totalPhysicalForkSize += forkPhysicalSize;
					}
				}
			} else {
				/* must be the data fork, then */
				if (fork != DISPLAY_FORK_RSRC) {
					if (totalLogicalForkSize) {
						*totalLogicalForkSize += forkLogicalSize;
					}
					if (totalPhysicalForkSize) {
						*totalPhysicalForkSize += forkPhysicalSize;
					}
				}
			}
		}
	} while (err == noErr);

	/* errFSNoMoreItems is not serious, we report other errors */
	if (err && err != errFSNoMoreItems) {
		return( err );
	} else {
		return( noErr );
	}
}

/*
//////////////////////////////////////
// Cut a file suffix of up to 4 characters
// from listed file names and return it
// It is then listed in a seperate column
// because of the -x option
//////////////////////////////////////
*/

/*
char* GetFileNameSuffix (char *name)
{
	short	len = strlen(name);
	short	i;

	for (i = 5; i > 0; i--) {
		if (name[len-i] == '.') {
			return (&name[len-i]);
		}
	}

	return NULL;
}
*/

/*//////////////////////////////////////
  // Transform OSType into a C string
  // An OSType is just a 4 character string
  // stored as a long integer
  /////////////////////////////////////*/
void OSTypeToStr( OSType aType, char *aStr )
{
	aStr[0] = (aType >> 24) & 0xFF;
	aStr[1] = (aType >> 16) & 0xFF;
	aStr[2] = (aType >> 8) & 0xFF;
	aStr[3] = aType & 0xFF;
	aStr[4] = 0;
}

/*
//////////////////////////////////////
// HFSUniStr255 converted to null-terminated C string
// For some inexplicable reason, there seems to be no
// convenient Apple function to do this without
// first converting to a Core Foundation string
//////////////////////////////////////
*/
static void HFSUniPStrToCString (HFSUniStr255 *uniStr, char *cstr)
{
	CFStringRef		cfStr;

	cfStr = CFStringCreateWithCharacters(kCFAllocatorDefault, uniStr->unicode, uniStr->length);

	CFStringGetCString( cfStr, cstr, 255, kCFStringEncodingUTF8 );
}
