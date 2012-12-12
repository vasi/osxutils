/*
    setlabel - Set the Finder color label of file
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
    
	0.2 - Updated to use exit code constants from sysexits.h, errors go to stderr
    0.1 - First release of setlabel

*/

/*  TODO

*/


/*
    Command line options

    v - version
    h - help - usage
    s - silent mode
    
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>
#include <string.h>
#include <sysexits.h>

///////////////  Definitions    //////////////

#define		PROGRAM_STRING  	"setlabel"
#define		VERSION_STRING		"0.2"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson <sveinbt@hi.is>"


/////////////// Prototypes  /////////////////

    // my stuff

    static void SetFileLabel (char *path, short labelNum);
    static short ParseLabelName (char *labelName);
    static short GetLabelNumber (short flags);
    static void PrintVersion (void);
    static void PrintHelp (void);
    static int UnixIsFolder (char *path);
    static void SetLabelInFlags (short *flags, short labelNum);
    static OSErr FSpGetPBRec(const FSSpec* fileSpec, CInfoPBRec *infoRec);
    
/////////////// Globals  /////////////////

const char	labelNames[8][7] = { "None", "Red", "Orange", "Yellow", "Green", "Blue", "Purple", "Gray" };
short		silentMode = 0;

int main (int argc, const char * argv[]) 
{
    int			rc;
    int			optch;
    static char	optstring[] = "vhs";
    char		*labelName;
    short		labelNum;

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
            case 's':
                silentMode = 1;
                break;
            default: // '?'
                rc = 1;
                return EX_OK;
        }
    }
    
    if (argc < 2)
    {
        fprintf(stderr, "Missing parameters.\n");
        PrintHelp();
        return EX_USAGE;
    }
    
    // get label identifying number from name string
	labelName = malloc(sizeof((char *)argv[optind]));
    strcpy((char *)labelName, (char *)argv[optind]);
    labelNum = ParseLabelName((char *)labelName);
	
    optind++;
    
    //all remaining arguments should be files
    for (; optind < argc; ++optind)
        SetFileLabel((char *)argv[optind], labelNum);
    
    return EX_OK;
}





#pragma mark -


///////////////////////////////////////////////////////////////////
// Make sure file exists and we have privileges.  Then set the 
// file Finder comment.
///////////////////////////////////////////////////////////////////
static void SetFileLabel (char *path, short labelNum)
{
    OSErr		err = noErr;
    FSRef		fileRef;
    FSSpec		fileSpec;
    short       isFldr;
    short       currentLabel;
    FInfo       finderInfo;
    CInfoPBRec  infoRec;

	//see if the file in question exists and we can write it
	if (access(path, R_OK|W_OK|F_OK) == -1)
	{
		perror(path);
		return;
	}
	
	//get file reference from path
	err = FSPathMakeRef((char *)path, &fileRef, NULL);
	if (err != noErr)
	{
		fprintf(stderr, "FSPathMakeRef: Error %d for file %s\n", err, path);
		return;
	}

	//retrieve filespec from file ref
	err = FSGetCatalogInfo (&fileRef, NULL, NULL, NULL, &fileSpec, NULL);
	if (err != noErr)
	{
		fprintf(stderr, "FSGetCatalogInfo(): Error %d getting file spec for %s\n", err, path);
		return;
	}
        
    /* Check if we're dealing with a folder */
    isFldr = UnixIsFolder(path);
    if (isFldr == -1)/* an error occurred in stat */
    {
            perror(path);
            return;
    }
    
    ///////////////////////// IF SPECIFIED FILE IS A FOLDER /////////////////////////
	
    if (isFldr)
    {
        //Get HFS record
        FSpGetPBRec(&fileSpec, &infoRec);
        
        //get current label
        currentLabel = GetLabelNumber(infoRec.dirInfo.ioDrUsrWds.frFlags);
        
        //set new label into record
        SetLabelInFlags(&infoRec.dirInfo.ioDrUsrWds.frFlags, labelNum);
        
        //fill in the requisite fields
        infoRec.hFileInfo.ioNamePtr = (unsigned char *)fileSpec.name;
		infoRec.hFileInfo.ioVRefNum = fileSpec.vRefNum;
		infoRec.hFileInfo.ioDirID = fileSpec.parID;
        
        //set the record
        PBSetCatInfoSync(&infoRec);
    }
    
    ///////////////////////// IF SPECIFIED FILE IS A REGULAR FILE /////////////////////////
	
    else
    {
        /* get the finder info */
        err = FSpGetFInfo (&fileSpec, &finderInfo);
        if (err != noErr) 
        {
            if (!silentMode)
                fprintf(stderr, "FSpGetFInfo(): Error %d getting file Finder info from file spec for file %s", err, path);
        }
        
        //retrieve the label number of the file
        currentLabel = GetLabelNumber(finderInfo.fdFlags);
        
        //if it's already set with the desired label we return
        if (currentLabel == labelNum)
            return;
        
        //set the appropriate value in the flags field
        SetLabelInFlags(&finderInfo.fdFlags, labelNum);
        
        //apply the settings to the file
        err = FSpSetFInfo (&fileSpec, &finderInfo);
        if (err != noErr)
        {
            if (!silentMode)
                fprintf(stderr, "FSpSetFInfo(): Error %d setting Finder info for %s", err, path);
            return;
        }
    }
    
    //print output reporting the changes made
	if (!silentMode)
		printf("%s:\n\t%s --> %s\n", path, (char *)&labelNames[currentLabel], (char *)&labelNames[labelNum]);
    
    return;
}


static void SetLabelInFlags (short *flags, short labelNum)
{
    short myFlags = *flags;

    //nullify former label
        /* Is Orange */
	if (myFlags & 2 && myFlags & 8 && myFlags & 4)
	{
		myFlags -= 2;
		myFlags -= 8;
		myFlags -= 4;
	}
        /* Is Red */
	if (myFlags & 8 && myFlags & 4)
	{
		myFlags -= 8;
		myFlags -= 4;
	}
        /* Is Yellow */
	if (myFlags & 8 && myFlags & 2)
	{
		myFlags -= 8;
		myFlags -= 2;
	}
        
        /* Is Blue */
	if (myFlags & 8)
	{
		myFlags -= 8;
	}
        
        /* Is Purple */
	if (myFlags & 2 && myFlags & 4)
	{
		myFlags -= 2;
		myFlags -= 4;
	}
        
        /* Is Green */
	if (myFlags & 4)
	{
		myFlags -= 4;
	}
        
        /* Is Gray */
	if (myFlags & 2)
	{
		myFlags -= 2;
	}
        
	//OK, now all the labels should be at off
	//create flags with the desired label
	switch(labelNum)
	{
		case 0://None
			break;
		case 2:
			myFlags += 2;
			myFlags += 8;
			myFlags += 4;
			break;
		case 1:
			myFlags += 8;
			myFlags += 4;
			break;
		case 3:
			myFlags += 2;
			myFlags += 8;
			break;
		case 5:
			myFlags += 8;
			break;
		case 6:
			myFlags += 2;
			myFlags += 4;
			break;
		case 4:
			myFlags += 4;
			break;
		case 7:
			myFlags += 2;
			break;
	}
        
    //now, to set the desired label
	*flags = myFlags;
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
    printf("usage: %s [-vhs] [label] [file ...]\n", PROGRAM_STRING);
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



static short ParseLabelName (char *labelName)
{
    short labelNum, i;
    
    for (i = 0; i < 8; i++)
    {
        if (!strcmp(labelName, (char *)labelNames[i]))
            labelNum = i;
    }

    return labelNum;
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


/*//////////////////////////////////////
// Returns directory info structure of 
// file spec
/////////////////////////////////////*/
static OSErr FSpGetPBRec(const FSSpec* fileSpec, CInfoPBRec *infoRec)
{
	CInfoPBRec                  myInfoRec = {0};
	OSErr                       err = noErr;

	myInfoRec.hFileInfo.ioNamePtr = (unsigned char *)fileSpec->name;
	myInfoRec.hFileInfo.ioVRefNum = fileSpec->vRefNum;
	myInfoRec.hFileInfo.ioDirID = fileSpec->parID;
	
	err = PBGetCatInfoSync(&myInfoRec);
    if (err == noErr)
		*infoRec = myInfoRec;
		
	return err;
}

/*//////////////////////////////////////
// Returns directory info structure of 
// file spec
/////////////////////////////////////*/
/*static OSErr FSpSetDInfo(const FSSpec* fileSpec, DInfo *dInfo)
{
	CInfoPBRec	infoRec = {0};
	OSErr		err = noErr;

        PBSetCatInfoSync(

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
*/