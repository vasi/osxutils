/*
    mkalias - command line program to create MacOS aliases
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


/*  CHANGES

	0.5 - * Sysexits.h used for return values
    
	0.4 - * Added support for creating relative aliases (see http://developer.apple.com/technotes/tn/tn1188.html for why this is useful)
		  * Fixed bug where mkalias would run without the required arguments and exit with major errors
	
    0.3 - * Added setting of type and creator code for files (courtesy of Chris Bailey <crb@users.sourceforge.net>)
	             * Can be switched off with the the -t flag
          * Changed use of NewAliasMinimal (deprecated) to FSNewAliasMinimal (courtesy of Chris Bailey <crb@users.sourceforge.net>)
          * Fixed ill-formed usage string
          * More informative error messages :)
    
    0.2 - * Added custom icon copying and the -c flag option to turn it off.
          * Internal methods made static.
    
    0.1 - * Initial release of mkalias

*/


/*  TODO
        
    * Ability to create symlink/alias combos
*/


/*
    Command line options

    v - version
    h - help - usage
    c - don't copy custom icon
	t - don't apply file and creator type of original
	r - make it a relative alias
    
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <Carbon/Carbon.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sysexits.h>


/////////////////// Definitions //////////////////

#define		PROGRAM_STRING  	"mkalias"
#define		VERSION_STRING		"0.5"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson"
#define		OPT_STRING			"vhctr"


/////////////////// Prototypes //////////////////

static void CreateAlias (char *srcPath, char *destPath);
static short UnixIsFolder (char *path);
static void PrintVersion (void);
static void PrintHelp (void);

///////////////// globals ////////////////////

short		noCustomIconCopy = false;
short		noCopyFileCreatorTypes = false;
short		makeRelativeAlias = false;

////////////////////////////////////////////
// main program function
////////////////////////////////////////////
int main (int argc, const char * argv[])
{
    int			rc;
    int			optch;
    static char	optstring[] = OPT_STRING;

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
            case 'c':
                noCustomIconCopy = true;
                break;
			case 't':
                noCopyFileCreatorTypes = true;
                break;
			case 'r':
				makeRelativeAlias = true;
				break;
            default: // '?'
                rc = 1;
                PrintHelp();
                return EX_USAGE;
        }
    }
    
    //check if a correct number of arguments was submitted
    if (argc - optind < 2)
    {
        fprintf(stderr,"Too few arguments.\n");
        PrintHelp();
        return EX_USAGE;
    }
    
    //check if file to be aliased exists
    if (access(argv[optind], F_OK) == -1)
    {
        perror(argv[optind]);
        return EX_NOINPUT;
    }
    
    //check if we can create alias in the specified location
    if (access(argv[optind+1], F_OK) != -1)
    {
        fprintf(stderr, "%s: File exists\n", argv[optind+1]);
        return EX_CANTCREAT;
    }
    
    //create the alias
	CreateAlias(/*source*/(char *)argv[optind], /*destination*/(char *)argv[optind+1]);
	
    return EX_OK;
}

#pragma mark -

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Given two path strings, srcPath and destPath, creates a MacOS Finder alias from file in srcPath
//  in the destPath, complete with custom icon and all.  Pretty neat.
//
//////////////////////////////////////////////////////////////////////////////////////////////////

static void CreateAlias (char *srcPath, char *destPath)
{
    OSErr		err;
    
    FSSpec		sourceFSSpec, destFSSpec;
    FSRef		srcRef, destRef;
    OSType		srcFileType = (OSType)NULL;
	OSType		srcCreatorType = (OSType)NULL;
    FInfo		srcFinderInfo, destFinderInfo;
    
    int			fd;
    SInt16		rsrcRefNum;
    
    IconRef				srcIconRef;
    IconFamilyHandle	srcIconFamily;
    SInt16				theLabel;
    
    AliasHandle		alias;
    short			isSrcFolder;
    
    //find out if we're dealing with a folder alias
    isSrcFolder = UnixIsFolder(srcPath);
    if (isSrcFolder == -1)//error
    {
        fprintf(stderr, "UnixIsFolder(): Error doing a stat on %s\n", srcPath);
        exit(EX_IOERR);
    }
    
    
    ///////////////////// Get the FSRef's and FSSpec's for source and dest ///////////////////
    
    
            //get file ref to src
            err = FSPathMakeRef(srcPath, &srcRef, NULL);
            if (err != noErr)
            {
                    fprintf(stderr, "FSPathMakeRef: Error %d getting file ref for source \"%s\"\n", err, srcPath);
                    exit(EX_IOERR);
            }
            
            //Apple's function to get FSRef from path refuses to create for nonexistent files, so
            //we just create a temporary file and the remove it to get FSRef for destination
            fd = open(destPath, O_CREAT | O_TRUNC | O_WRONLY);
            
            //file creation fails
            if (fd == -1)//this can't happen, but just to be sure...
            {
                fprintf(stderr, "open(2): Error %d creating %s\n", errno, destPath);
                exit(EX_CANTCREAT);
            }
            close(fd);
            
            //get file ref to dest
            err = FSPathMakeRef(destPath, &destRef, NULL);
            if (err != noErr)
            {
                    fprintf(stderr, "FSPathMakeRef: Error %d getting file ref for dest \"%s\"\n", err, destPath);
                    exit(EX_IOERR);
            }
            
            //retrieve source filespec from source file ref
            err = FSGetCatalogInfo (&srcRef, NULL, NULL, NULL, &sourceFSSpec, NULL);
            if (err != noErr)
            {
                fprintf(stderr, "FSGetCatalogInfo(): Error %d getting file spec from source FSRef\n", err);
                exit(EX_IOERR);
            }
			
            //retrieve dest filespec from dest file ref
            err = FSGetCatalogInfo (&destRef, NULL, NULL, NULL, &destFSSpec, NULL);
            if (err != noErr)
            {
                printf("FSGetCatalogInfo(): Error %d getting file spec from dest FSRef\n", err);
                exit(EX_IOERR);
            }
            
            unlink(destPath);//temp file removed
    
            //get the finder info for the source if it's a folder
            if (!isSrcFolder)
            {
                    err = FSpGetFInfo (&sourceFSSpec, &srcFinderInfo);
                    if (err != noErr)
                    {
                        fprintf(stderr, "FSpGetFInfo(): Error %d getting Finder info for source \"%s\"\n", err, srcPath);
                        exit(EX_IOERR);
                    }
                    srcFileType = srcFinderInfo.fdType;
                    srcCreatorType = srcFinderInfo.fdCreator;
            }
    
    //////////////// Get the source file's icon ///////////////////////
    
        if (!noCustomIconCopy)
        {
            err = GetIconRefFromFile (&sourceFSSpec, &srcIconRef, &theLabel);
            if (err != noErr)
            {
                fprintf(stderr, "GetIconRefFromFile(): Error getting source file's icon.\n");
            }
            IconRefToIconFamily (srcIconRef, kSelectorAllAvailableData, &srcIconFamily);
        }

    ///////////////////// Create the relevant alias record ///////////////////
    
	if (makeRelativeAlias)
	{
		// The following code for making relative aliases was borrowed from Apple. See the following technote:
		// 
		//  http://developer.apple.com/technotes/tn/tn1188.html
		//
		
		// create the new file
		FSpCreateResFile(&destFSSpec, 'TEMP', 'TEMP', smSystemScript);
		if ((err = ResError()) != noErr)
		{
			fprintf(stderr, "FSpCreateResFile(): Error %d while creating file\n", err);
			exit(EX_CANTCREAT);
		}

		//create the alias record, relative to the new alias file
		err = NewAlias(&destFSSpec, &sourceFSSpec, &alias);
		if (err != noErr)
		{
			fprintf(stderr, "NewAlias(): Error %d while creating relative alias\n", err);
			exit(EX_CANTCREAT);
		}
    	
		// save the resource
		rsrcRefNum = FSpOpenResFile(&destFSSpec, fsRdWrPerm);
		if (rsrcRefNum == -1) 
		{ 
			err = ResError(); 
			fprintf(stderr, "Error %d while opening resource fork for %s\n", err, (char *)&destPath);
			exit(EX_IOERR);
		}
		UseResFile(rsrcRefNum);
		AddResource((Handle) alias, rAliasType, 0, destFSSpec.name);
		if ((err = ResError()) != noErr)
		{
			fprintf(stderr, "Error %d while adding alias resource for %s", err, (char *)&destPath);
			exit(EX_IOERR);
		}
		if (!noCustomIconCopy)
		{
			//write the custom icon data
            AddResource( (Handle)srcIconFamily, kIconFamilyType, kCustomIconResource, "\p");
        }
                
		CloseResFile(rsrcRefNum);
	}
	else
	{
            //create alias record from source spec
            FSNewAliasMinimal (&srcRef, &alias);
            if (alias == NULL)
            {
                fprintf(stderr, "NewAliasMinimal(): Null handle Alias returned from FSRef for file %s\n", sourceFSSpec.name);
                exit(EX_IOERR);
            }
			
			// Create alias resource file
            // If we're dealing with a folder, we use the Finder types for folder
            // Otherwise, we use the same File/Creator as source file
            
            if (isSrcFolder)
                FSpCreateResFile(&destFSSpec, 'MACS', 'fdrp', -1);
            else
                FSpCreateResFile(&destFSSpec, '    ', '    ', -1);
            
            //open resource file and write the relevant resources
            rsrcRefNum = FSpOpenResFile (&destFSSpec, 3);
            
                //write the alias resource
                AddResource ((Handle)alias, 'alis', 0, NULL);
                
                if (!noCustomIconCopy)
                {
                    //write the custom icon data
                    AddResource( (Handle)srcIconFamily, kIconFamilyType, kCustomIconResource, "\p");
                }
                
            CloseResFile(rsrcRefNum);				
     }       

    
    ///////////////////// Set the relevant finder flags for alias ///////////////////
    
    
            //get finder info on newly created alias
            err = FSpGetFInfo (&destFSSpec, &destFinderInfo);
            if (err != noErr)
            {
                printf("FSpGetFInfo(): Error %d getting Finder info for target alias \"%s\"\n", err, srcPath);
                exit(EX_IOERR);
            }
            
            // set the flags in finder data
            // we set both alias flag and custom icon flag
            //
            if (!noCustomIconCopy)
                destFinderInfo.fdFlags = destFinderInfo.fdFlags | 0x8000 | kHasCustomIcon;
            else
                destFinderInfo.fdFlags = destFinderInfo.fdFlags | 0x8000;
            
            //if it's not a folder alias the alias gets the same creator/type as original
            if (!isSrcFolder && !noCopyFileCreatorTypes) 
            {
 	            destFinderInfo.fdType = srcFileType;
     	        destFinderInfo.fdCreator = srcCreatorType;
     	    }
            
            err = FSpSetFInfo (&destFSSpec, &destFinderInfo);
            if (err != noErr)
            {
                printf("FSpSetFInfo(): Error %d setting Finder Alias flag (0x8000).\n", err);
                exit(EX_IOERR);
            }
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
    printf("usage: %s [-%s] [source-file] [target-alias]\n", PROGRAM_STRING, OPT_STRING);
}

