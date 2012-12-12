/*
    hfsdata - print out Mac OS HFS+ meta-data for a file
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
    
    0.1 - First release of hfsdata

*/

/*
    Command line options

    v - version
    h - help - usage

	//hfsdata flags

	-x	tells whether file suffix is hidden			DONE
	
	-A	tells what application is used to open file	DONE
	
	-c	date created								DONE
	-m	date modified								DONE
	-a	date accessed								DONE
	-t	date of attribute modification				DONE
	
	-r	Resource fork size, logical					DONE
	-R	Resource fork size, physical				DONE
	-s	Total size of both forks, logical			DONE
	-S	Total size of both forks, physical			DONE
	-d	Data fork size, logical						DONE
	-D	Data fork size, physical					DONE
	
	-T	file type code								DONE
	-C	creator type code							DONE
	
	-k	file kind, as it appears in the Finder		DONE
	
	-l	label, numerically							DONE
	-L	label, by name								DONE
	
	-o	Mac OS X Finder comment						DONE
	-O	Mac OS 9 Finder comment						DONE
	
	-e	Show file pointed to by alias				DONE
    
*/


#define	kSuffixHidden				0
#define	kAppForFile					1
#define	kDateCreated				2
#define	kDateModified				3
#define	kDateAccessed				4
#define	kDateAttrMod				5
#define	kLogicalResourceForkSize	6
#define	kPhysicalResourceForkSize	7
#define	kLogicalTotalForkSize		8
#define	kPhysicalTotalForkSize		9
#define	kLogicalDataForkSize		10
#define	kPhysicalDataForkSize		11
#define	kFileTypeCode				12
#define	kCreatorTypeCode			13
#define	kFileKind					14
#define	kLabelNumeric				15
#define	kLabelName					16
#define	kMacOSXComment				17
#define	kMacOS9Comment				18
#define	kAliasOriginal				19

/////////////// Includes /////////////////

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <Carbon/Carbon.h>
#include <string.h>

////////////// Prototypes ////////////////

	static OSErr PrintIsExtensionHidden (FSRef *fileRef);
	static OSErr PrintAliasSource (FSRef *fileRef);
	static OSErr PrintResourceForkLogicalSize (FSRef *fileRef);
	static OSErr PrintResourceForkPhysicalSize (FSRef *fileRef);
	static OSErr PrintDataForkLogicalSize (FSRef *fileRef);
	static OSErr PrintDataForkPhysicalSize (FSRef *fileRef);
	static OSErr PrintBothForksLogicalSize (FSRef *fileRef);
	static OSErr PrintBothForksPhysicalSize (FSRef *fileRef);
	static OSErr PrintDateCreated (FSRef *fileRef);
	static OSErr PrintDateContentModified (FSRef *fileRef);
	static OSErr PrintDateLastAccessed (FSRef *fileRef);
	static OSErr PrintDateAttributeModified (FSRef *fileRef);
	static OSErr PrintFileType (FSRef *fileRef);
	static OSErr PrintCreatorCode (FSRef *fileRef);
	static OSErr PrintKind (FSRef *fileRef);
	static OSErr PrintAppWhichOpensFile (FSRef *fileRef);
	static OSErr PrintLabelName (FSRef *fileRef);
	static OSErr PrintLabelNumber (FSRef *fileRef);
	
	static void OSTypeToStr(OSType aType, char *aStr);
	static int UnixIsFolder (char *path);
	static Boolean IsFolder (FSRef *fileRef);
	static void HFSUniPStrToCString (HFSUniStr255 *uniStr, char *cstr);
	static OSStatus FSMakePath(FSRef fileRef, UInt8 *path, UInt32 maxPathSize);
	static OSErr FSpGetDInfo(const FSSpec* fileSpec, DInfo *dInfo);
	static short GetLabelNumber (short flags);
	static OSErr GetDateTimeStringFromUTCDateTime (UTCDateTime *utcDateTime, char *dateTimeString);
	
	static void PrintUsage (void);
	static void PrintVersion (void);
	static void PrintHelp (void);
	
	static OSErr PrintOSXComment (FSRef	*fileRef);
	static OSErr PrintOS9Comment (FSRef *fileRef);
	
	//AE functions from MoreAppleEvents.c, Apple's sample code
	pascal OSErr MoreFEGetComment(const FSSpecPtr pFSSpecPtr,Str255 pCommentStr,const AEIdleUPP pIdleProcUPP);
	pascal void MoreAEDisposeDesc(AEDesc* desc);
	pascal void MoreAENullDesc(AEDesc* desc);
	pascal OSStatus MoreAEOCreateObjSpecifierFromFSSpec(const FSSpecPtr pFSSpecPtr,AEDesc *pObjSpecifier);
	pascal OSStatus MoreAEOCreateObjSpecifierFromFSRef(const FSRefPtr pFSRefPtr,AEDesc *pObjSpecifier);
	pascal OSStatus MoreAEOCreateObjSpecifierFromCFURLRef(const CFURLRef pCFURLRef,AEDesc *pObjSpecifier);
	pascal OSStatus MoreAESendEventReturnAEDesc(const AEIdleUPP    pIdleProcUPP, const AppleEvent  *pAppleEvent,const DescType    pDescType, AEDesc        *pAEDesc);
	pascal OSStatus MoreAESendEventReturnPString(const AEIdleUPP pIdleProcUPP,const AppleEvent* pAppleEvent,Str255 pStr255);
	pascal  OSStatus  MoreAEGetHandlerError(const AppleEvent* pAEReply);
	pascal OSStatus MoreAESendEventReturnData(const AEIdleUPP    pIdleProcUPP,const AppleEvent  *pAppleEvent,DescType      pDesiredType,DescType*      pActualType,void*         pDataPtr,Size        pMaximumSize,Size         *pActualSize);
	pascal OSErr MoreAEGetCFStringFromDescriptor(const AEDesc* pAEDesc, CFStringRef* pCFStringRef);
	Boolean MyAEIdleCallback (EventRecord * theEvent,SInt32 * sleepTime,RgnHandle * mouseRgn);


// Some MoreAppleEvents stuff I don't understand and don't want to
            #define MoreAssert(x) (true)
            #define MoreAssertQ(x)




///////////////  Definitions    //////////////

#define		MAX_COMMENT_LENGTH	255
#define		PROGRAM_STRING  	"hfsdata"
#define		VERSION_STRING		"0.1"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson"
#define     USAGE_STRING        "hfsdata [-x|A|c|m|a|t|r|R|s|S|d|D|T|C|k|l|L|o|O|e] file\nor\nhfsdata [-hv]\n"

// The Mac Four-Character Application Signature for the Finder
static const OSType gFinderSignature = 'MACS';

int main (int argc, const char * argv[]) 
{
	OSErr		err = noErr;
    int			rc;
    int			optch;
	char		*path;
	FSRef		fileRef;
	int			type;
    static char	optstring[] = "vhxAcmatrRsSdDTCklLoOe";

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
			case 'x':
				type = kSuffixHidden;
				break;
			case 'A':
				type = kAppForFile;
				break;
			case 'c':
				type = kDateCreated;
				break;
			case 'm':
				type = kDateModified;
				break;
            case 'a':
				type = kDateAccessed;
				break;
			case 't':
				type = kDateAttrMod;
				break;
			case 'r':
				type = kLogicalResourceForkSize;
				break;
			case 'R':
				type = kPhysicalResourceForkSize;
				break;
			case 's':
				type = kLogicalTotalForkSize;
				break;
			case 'S':
				type = kPhysicalTotalForkSize;
				break;
			case 'd':
				type = kLogicalDataForkSize;
				break;
			case 'D':
				type = kPhysicalDataForkSize;
				break;
			case 'T':
				type = kFileTypeCode;
				break;
			case 'C':
				type = kCreatorTypeCode;
				break;
			case 'k':
				type = kFileKind;
				break;
			case 'l':
				type = kLabelNumeric;
				break;
			case 'L':
				type = kLabelName;
				break;
			case 'o':
				type = kMacOSXComment;
				break;
			case 'O':
				type = kMacOS9Comment;
				break;
			case 'e':
				type = kAliasOriginal;
				break;
			default: // '?'
                rc = 1;
                PrintUsage();
                return 0;
        }
    }
	
	
    
	//path to file passed as argument
	path = (char *)argv[optind];
	if (path == NULL)
	{
		PrintHelp();
		exit(0);
	}
	
	if (access(path, R_OK|F_OK) == -1)
	{
		perror(path);
		exit(1);
	}
	
	// Get file ref to the file or folder pointed to by the path
    err = FSPathMakeRef((unsigned char *)path, &fileRef, NULL);
	if (err != noErr) 
    {
        fprintf(stderr, "FSPathMakeRef(): Error %d returned when getting file reference for %s\n", err, path);
		exit(1);
    }
	
	switch(type)
	{
		case kSuffixHidden:
			err = PrintIsExtensionHidden(&fileRef);
			break;
		case kAppForFile:
			err = PrintAppWhichOpensFile(&fileRef);
			break;
		case kDateCreated:
			err = PrintDateCreated(&fileRef);
			break;
		case kDateModified:
			err = PrintDateContentModified(&fileRef);
			break;
		case kDateAccessed:
			err = PrintDateLastAccessed(&fileRef);
			break;
		case kDateAttrMod:
			err = PrintDateAttributeModified(&fileRef);
			break;
		case kLogicalResourceForkSize:
			err = PrintResourceForkLogicalSize(&fileRef);
			break;
		case kPhysicalResourceForkSize:
			err = PrintResourceForkPhysicalSize(&fileRef);
			break;
		case kLogicalTotalForkSize:
			err = PrintBothForksLogicalSize(&fileRef);
			break;
		case kPhysicalTotalForkSize:
			err = PrintBothForksPhysicalSize(&fileRef);
			break;
		case kLogicalDataForkSize:
			err = PrintDataForkLogicalSize(&fileRef);
			break;
		case kPhysicalDataForkSize:
			err = PrintDataForkPhysicalSize(&fileRef);
			break;
		case kFileTypeCode:
			err = PrintFileType(&fileRef);
			break;
		case kCreatorTypeCode:
			err = PrintCreatorCode(&fileRef);
			break;
		case kFileKind:
			err = PrintKind(&fileRef);
			break;
		case kLabelNumeric:
			err = PrintLabelNumber(&fileRef);
			break;
		case kLabelName:
			err = PrintLabelName(&fileRef);
			break;
		case kMacOSXComment:
			err = PrintOSXComment(&fileRef);
			break;
		case kMacOS9Comment:
			err = PrintOS9Comment(&fileRef);
			break;
		case kAliasOriginal:
			err = PrintAliasSource(&fileRef);
			break;
	}
	
	exit(err);

	return err;
}

#pragma mark -

////////////////////////////////////////
// Print whether the file is set to show
// its suffix in the filename
//////////////////////////////////////
static OSErr PrintIsExtensionHidden (FSRef *fileRef)
{
	LSItemInfoRecord	infoRecord;
	OSErr				err = noErr;
	
	err = LSCopyItemInfoForRef(fileRef, kLSRequestExtensionFlagsOnly, &infoRecord);
	if (err)
	{	
		fprintf(stderr, "Error %d in LSCopyItemInfoForRef()\n", err);
		return err;
	}
			
	if (infoRecord.flags & kLSItemInfoExtensionIsHidden)
		printf("Yes\n");
	else
		printf("No\n");
	
	return err;
}

///////////////////////////////////////
// On being passed the path to a MacOS alias,
// returns path to the original file to which
// it refers.  Returns null if file not found.
//////////////////////////////////////
static OSErr PrintAliasSource (FSRef *fileRef)
{
    OSErr	err = noErr;
    static char	srcPath[2048];
    Boolean	isAlias, isFolder;
	FSRef	aliasRef;
    
    //make sure we're dealing with an alias
    err = FSIsAliasFile (fileRef, &isAlias, &isFolder);
    if (err != noErr)
    {
        fprintf(stderr, "Error determining alias properties.\n");
        return err;
    }
    if (!isAlias)
    {
        fprintf(stderr, "Argument is not an alias.\n");
        return TRUE;
    }
    
    //resolve alias --> get file reference to file
    err = FSResolveAliasFile (fileRef, TRUE, &isFolder, &isAlias);
    if (err != noErr)
    {
        fprintf(stderr, "Error resolving alias.\n");
        return err;
    }
    
    //get path to file that alias points to
    err = FSMakePath(*fileRef, (char *)&srcPath, strlen(srcPath));
    if (err != noErr)
	{
		fprintf(stderr, "Error getting path from file reference\n");
		return err;
    }
	printf("%s\n", &srcPath);
}






#pragma mark -

static OSErr PrintResourceForkLogicalSize (FSRef *fileRef)
{
	OSErr				err = noErr;
	FSCatalogInfoBitmap cinfoMap = kFSCatInfoRsrcSizes;
	FSCatalogInfo		cinfo;
	
	err = FSGetCatalogInfo (fileRef, cinfoMap, &cinfo, NULL, NULL, NULL);
	if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d returned when retrieving catalog information\n", err);
        return err;
    }
	
	printf("%llu\n", cinfo.rsrcLogicalSize);
	return err;
}

static OSErr PrintResourceForkPhysicalSize (FSRef *fileRef)
{
	OSErr				err = noErr;
	FSCatalogInfoBitmap cinfoMap = kFSCatInfoRsrcSizes;
	FSCatalogInfo		cinfo;
	
	err = FSGetCatalogInfo (fileRef, cinfoMap, &cinfo, NULL, NULL, NULL);
	if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d returned when retrieving catalog information\n", err);
        return err;
    }
	
	printf("%llu\n", cinfo.rsrcPhysicalSize);
	return err;
}


static OSErr PrintDataForkLogicalSize (FSRef *fileRef)
{
	OSErr				err = noErr;
	FSCatalogInfoBitmap cinfoMap = kFSCatInfoDataSizes;
	FSCatalogInfo		cinfo;
	
	err = FSGetCatalogInfo (fileRef, cinfoMap, &cinfo, NULL, NULL, NULL);
	if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d returned when retrieving catalog information\n", err);
        return err;
    }
	
	printf("%llu\n", cinfo.dataLogicalSize);
	return err;
}


static OSErr PrintDataForkPhysicalSize (FSRef *fileRef)
{
	OSErr				err = noErr;
	FSCatalogInfoBitmap cinfoMap = kFSCatInfoDataSizes;
	FSCatalogInfo		cinfo;
	
	err = FSGetCatalogInfo (fileRef, cinfoMap, &cinfo, NULL, NULL, NULL);
	if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d returned when retrieving catalog information\n", err);
        return err;
    }
	
	printf("%llu\n", cinfo.dataPhysicalSize);
	return err;
}

static OSErr PrintBothForksLogicalSize (FSRef *fileRef)
{
	OSErr				err = noErr;
	FSCatalogInfoBitmap cinfoMap = kFSCatInfoDataSizes+kFSCatInfoRsrcSizes;
	FSCatalogInfo		cinfo;
	
	err = FSGetCatalogInfo (fileRef, cinfoMap, &cinfo, NULL, NULL, NULL);
	if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d returned when retrieving catalog information\n", err);
        return err;
    }
	
	printf("%llu\n", cinfo.rsrcLogicalSize + cinfo.dataLogicalSize);
	return err;
}

static OSErr PrintBothForksPhysicalSize (FSRef *fileRef)
{
	OSErr				err = noErr;
	FSCatalogInfoBitmap cinfoMap = kFSCatInfoDataSizes+kFSCatInfoRsrcSizes;
	FSCatalogInfo		cinfo;
	
	err = FSGetCatalogInfo (fileRef, cinfoMap, &cinfo, NULL, NULL, NULL);
	if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d returned when retrieving catalog information\n", err);
        return err;
    }
	
	printf("%llu\n", cinfo.dataPhysicalSize + cinfo.rsrcPhysicalSize);
	return err;
}

#pragma mark -

static OSErr PrintDateCreated (FSRef *fileRef)
{
	OSErr				err = noErr;
	FSCatalogInfoBitmap cinfoMap = kFSCatInfoCreateDate;
	FSCatalogInfo		cinfo;
	char				dateCreatedString[255];
	
	err = FSGetCatalogInfo (fileRef, cinfoMap, &cinfo, NULL, NULL, NULL);
	if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d returned when retrieving catalog information\n", err);
        return err;
    }
	
	err = GetDateTimeStringFromUTCDateTime(&cinfo.createDate, (char *)&dateCreatedString);
	if (err != noErr)
	{
		fprintf(stderr, "GetDateTimeStringFromUTCDateTime(): Error %d generating date string\n", err);
        return err;
	}
	printf("%s\n", &dateCreatedString);
	
	return err;
}

static OSErr PrintDateContentModified (FSRef *fileRef)
{
	OSErr				err = noErr;
	FSCatalogInfoBitmap cinfoMap = kFSCatInfoContentMod;
	FSCatalogInfo		cinfo;
	char				dateString[255];
	
	err = FSGetCatalogInfo (fileRef, cinfoMap, &cinfo, NULL, NULL, NULL);
	if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d returned when retrieving catalog information\n", err);
        return err;
    }
	
	err = GetDateTimeStringFromUTCDateTime(&cinfo.contentModDate, (char *)&dateString);
	if (err != noErr)
	{
		fprintf(stderr, "GetDateTimeStringFromUTCDateTime(): Error %d generating date string\n", err);
        return err;
	}
	printf("%s\n", &dateString);
	
	return err;
}

static OSErr PrintDateLastAccessed (FSRef *fileRef)
{
	OSErr				err = noErr;
	FSCatalogInfoBitmap cinfoMap = kFSCatInfoAccessDate;
	FSCatalogInfo		cinfo;
	char				dateString[255];
	
	err = FSGetCatalogInfo (fileRef, cinfoMap, &cinfo, NULL, NULL, NULL);
	if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d returned when retrieving catalog information\n", err);
        return err;
    }
	
	err = GetDateTimeStringFromUTCDateTime(&cinfo.accessDate, (char *)&dateString);
	if (err != noErr)
	{
		fprintf(stderr, "GetDateTimeStringFromUTCDateTime(): Error %d generating date string\n", err);
        return err;
	}
	printf("%s\n", &dateString);
	
	return err;
}

static OSErr PrintDateAttributeModified (FSRef *fileRef)
{
	OSErr				err = noErr;
	FSCatalogInfoBitmap cinfoMap = kFSCatInfoAttrMod;
	FSCatalogInfo		cinfo;
	char				dateString[255];
	
	err = FSGetCatalogInfo (fileRef, cinfoMap, &cinfo, NULL, NULL, NULL);
	if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d returned when retrieving catalog information\n", err);
        return err;
    }
	
	err = GetDateTimeStringFromUTCDateTime(&cinfo.attributeModDate, (char *)&dateString);
	if (err != noErr)
	{
		fprintf(stderr, "GetDateTimeStringFromUTCDateTime(): Error %d generating date string\n", err);
        return err;
	}
	printf("%s\n", &dateString);
	
	return err;
}

#pragma mark -

static OSErr PrintFileType (FSRef *fileRef)
{
	FSSpec		fileSpec;
    FInfo 		finderInfo;
	OSErr		err = noErr;
	char		fileType[5] = "\0";

	//if it's a folder
	if (IsFolder(fileRef))
	{
		printf("fold\n");
		return 0;
	}

	// retrieve filespec from file ref
    err = FSGetCatalogInfo (fileRef, NULL, NULL, NULL, &fileSpec, NULL);
    if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d getting file spec from file reference\n", err);
        return err;
    }
	// get Finder File Info
	err = FSpGetFInfo (&fileSpec, &finderInfo);
	if (err != noErr)
	{
		fprintf(stderr, "FSpGetFInfo(): Error %d getting file Finder File Info from file spec\n", err);
		return err;
	}
	
	/* get creator type string */
	OSTypeToStr(finderInfo.fdType, fileType);
	
	//print it
	if (strlen(fileType) != 0)
		printf("%s\n", fileType);
	
	return 0;
}


static OSErr PrintCreatorCode (FSRef *fileRef)
{
	FSSpec		fileSpec;
    FInfo 		finderInfo;
	OSErr		err = noErr;
	char		creatorType[5] = "\0";
	
	//if it's a folder
	if (IsFolder(fileRef))
	{
		printf("MACS\n");
		return 0;
	}

	// retrieve filespec from file ref
    err = FSGetCatalogInfo (fileRef, NULL, NULL, NULL, &fileSpec, NULL);
    if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d getting file spec from file reference\n", err);
        return err;
    }
	// get Finder File Info
	err = FSpGetFInfo (&fileSpec, &finderInfo);
	if (err != noErr)
	{
		fprintf(stderr, "FSpGetFInfo(): Error %d getting file Finder File Info from file spec\n", err);
		return err;
	}
	
	/* get creator type string */
	OSTypeToStr(finderInfo.fdCreator, creatorType);
	
	//print it
	if (strlen(creatorType) != 0)
		printf("%s\n", creatorType);

	return 0;
}

#pragma mark -

static OSErr PrintKind (FSRef *fileRef)
{
	OSErr				err = noErr;
	CFStringRef			kindString;
	char				cKindStr[1024];
	
	err = LSCopyKindStringForRef(fileRef, &kindString);
	if (err)
		return err;
	
	CFStringGetCString(kindString, (char *)&cKindStr, 1024, CFStringGetSystemEncoding());
	
	printf("%s\n", cKindStr);
	return 0;
}

static OSErr PrintAppWhichOpensFile (FSRef *fileRef)
{
	char		appPath[2048];
	FSRef		appRef;
	LSRolesMask	roleMask;
	OSErr		err = noErr;

	//make sure it's a file, not a folder
	if (IsFolder(fileRef))
	{
		fprintf(stderr, "Designated path is a folder\n");
		return TRUE;
	}

	//get the application which opens this item
	err = LSGetApplicationForItem (fileRef, roleMask, &appRef,NULL);
	if (err == kLSApplicationNotFoundErr)
	{
		printf("This file has no preferred application set.\n", err);
		return 0;
	}
	if (err != noErr)
	{
		fprintf(stderr, "An error of type %d occurred in LSGetApplicationForItem()\n", err);
		return err;
	}

	//get path from file reference
	FSRefMakePath(&appRef, (char *)&appPath, 2048);
	if (err != noErr)
	{
		fprintf(stderr, "An error of type %d occurred in FSRefMakePath()\n", err);
		return err;
	}

	//print out path to application which will open file
	printf("%s\n", appPath);
	return noErr;
}

#pragma mark -

static OSErr PrintLabelName (FSRef *fileRef)
{
	static char	labelNames[8][8] = { "None", "Red", "Orange", "Yellow", "Green", "Blue", "Purple", "Gray" };
	FSSpec		fileSpec;
    FInfo 		finderInfo;
	DInfo       dInfo;
	OSErr		err = noErr;
	int			labelNum = 0;

	/* retrieve filespec from file ref */
    err = FSGetCatalogInfo (fileRef, NULL, NULL, NULL, &fileSpec, NULL);
    if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d getting file spec from file reference\n", err);
        return err;
    }
	
	if (IsFolder(fileRef))
	{
		err = FSpGetDInfo (&fileSpec, &dInfo);
		if (err != noErr)
		{
			fprintf(stderr, "FSpGetFInfo(): Error %d getting file Finder Directory Info from file spec\n", err);
			return err;
		}
		labelNum = GetLabelNumber(dInfo.frFlags);
	}
	else
	{
		err = FSpGetFInfo (&fileSpec, &finderInfo);
		if (err != noErr)
		{
			fprintf(stderr, "FSpGetFInfo(): Error %d getting file Finder File Info from file spec\n", err);
			return err;
		}
		labelNum = GetLabelNumber(finderInfo.fdFlags);
	}
	
	printf("%s\n", (char *)&labelNames[labelNum]);
	
	return noErr;
}

static OSErr PrintLabelNumber (FSRef *fileRef)
{
	FSSpec		fileSpec;
    FInfo 		finderInfo;
	DInfo       dInfo;
	OSErr		err = noErr;
	int			labelNum = 0;

	/* retrieve filespec from file ref */
    err = FSGetCatalogInfo (fileRef, NULL, NULL, NULL, &fileSpec, NULL);
    if (err != noErr) 
    {
        fprintf(stderr, "FSGetCatalogInfo(): Error %d getting file spec from file reference\n", err);
        return err;
    }
	
	if (IsFolder(fileRef))
	{
		err = FSpGetDInfo (&fileSpec, &dInfo);
		if (err != noErr)
		{
			fprintf(stderr, "FSpGetFInfo(): Error %d getting file Finder Directory Info from file spec\n", err);
			return err;
		}
		labelNum = GetLabelNumber(dInfo.frFlags);
	}
	else
	{
		err = FSpGetFInfo (&fileSpec, &finderInfo);
		if (err != noErr)
		{
			fprintf(stderr, "FSpGetFInfo(): Error %d getting file Finder File Info from file spec\n", err);
			return err;
		}
		labelNum = GetLabelNumber(finderInfo.fdFlags);
	}
	
	printf("%d\n", labelNum);
	
	return noErr;
}

#pragma mark -

/*//////////////////////////////////////
// Transform OSType into a C string
// An OSType is just a 4 character string
// stored as a long integer
/////////////////////////////////////*/
static void OSTypeToStr(OSType aType, char *aStr)
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
    
    err = stat(path, &filestat);
    if (err == -1)
        return err;

    return (S_ISREG(filestat.st_mode) != 1);
}


////////////////////////////////////////
// Check if FSRef is a folder
///////////////////////////////////////
static Boolean IsFolder (FSRef *fileRef)
{
	FSCatalogInfo 	catalogInfo;

	FSGetCatalogInfo(fileRef, kFSCatInfoNodeFlags, &catalogInfo, NULL, NULL, NULL);
	if ((catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) == kFSNodeIsDirectoryMask )
		return true;
	else
		return false;
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


static OSErr GetDateTimeStringFromUTCDateTime (UTCDateTime *utcDateTime, char *dateTimeString)
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
	
	CopyPascalStringToC (pDateStr, (char *)&dateStr);
	CopyPascalStringToC (pTimeStr, (char *)&timeStr);
	
	strcpy(dateTimeString, (char *)&timeStr);
	strcat(dateTimeString, (char *)&" ");
	strcat(dateTimeString, (char *)&dateStr);
	
	return err;
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
// Print usage string
/////////////////////////////////////*/

static void PrintUsage (void)
{
    fprintf(stderr, "usage: %s\n", USAGE_STRING);
}

/*//////////////////////////////////////
// Print help string to stdout
/////////////////////////////////////*/
static void PrintHelp (void)
{
	puts("hfsdata - retrieve Mac meta-data for a file or folder");
	puts("");
	puts("Supported flags:");
	puts("");
	puts("\t-e  Prints the path of the file pointed to by a given alias");
	puts("\t-x  Prints whether file's suffix is hidden by the Finder or not");
	puts("\t-A  Prints the name of the file's preferred application");
	puts("");
	puts("\t-c  Prints date created in standard format");
	puts("\t-m  Prints date modified in standard format");
	puts("\t-a  Prints date accessed in standard format");
	puts("\t-t  Prints date attribute was modified in standard format");
	puts("");
	puts("\t-r  Prints logical resource fork size in bytes");
	puts("\t-R  Prints physical resource fork size in bytes");
	puts("\t-s  Prints total logical size of all forks in bytes");
	puts("\t-S  Prints total physical size of all forks in bytes");
	puts("\t-d  Prints logical data fork size in bytes");
	puts("\t-D  Prints physical data fork size in bytes");
	puts("");
	puts("\t-T  Prints the file's 4-character type code");
	puts("\t-C  Prints the file's 4-character creator code");
	puts("");
	puts("\t-k  Prints the file's 'Kind' name, as it appears in the Finder");
	puts("");
	puts("\t-l  Prints the file's label as a number (i.e. 0-8)");
	puts("\t-L  Prints the file's label as a name (e.g. Green)");
	puts("");
	puts("\t-o  Prints the file's Mac OS X Finder comment");
	puts("\t-O  Prints the file's Mac OS 9 Desktop Database comment");
	puts("");
	
}

#pragma mark -

static OSErr PrintOSXComment (FSRef	*fileRef)
{
	OSErr	err = noErr;
    FSSpec	fileSpec;
	Str255	comment = "\p";
	char	cStrCmt[255] = "\0";
	AEIdleUPP inIdleProc = NewAEIdleUPP(&MyAEIdleCallback);
        
            //retrieve filespec from file ref
            err = FSGetCatalogInfo (fileRef, NULL, NULL, NULL, &fileSpec, NULL);
            if (err != noErr)
            {
				fprintf(stderr, "FSGetCatalogInfo(): Error %d getting file spec for %s\n", err);
                return err;
            }
    
    ///////////// oK, now we can go about getting the comment /////////////

	// call the apple event routine. I'm not going to pretend I understand what's going on
	// in all those horribly kludgy functions, but at least it works.
	err = MoreFEGetComment(&fileSpec, comment, inIdleProc);
	if (err)
	{
		fprintf(stderr, "Error %d getting comment\n", err);
		if (err == -600)
			fprintf(stderr, "Finder is not running\n", err);//requires Finder to be running
		return err;
	}
	//convert pascal string to c string
	strncpy((char *)&cStrCmt, (unsigned char *)&comment+1, comment[0]);
	
	//if there is a comment, we print it
	if (strlen((char *)&cStrCmt))
		printf("%s\n", (char *)&cStrCmt);
		
	return noErr;
}



static OSErr PrintOS9Comment (FSRef *fileRef)
{
	OSErr	err = noErr;
    FSSpec	fileSpec;
	DTPBRec dt;
	char	buf[255] = "\0";
	char	comment[255] = "\0";

	//retrieve filespec from file ref
	err = FSGetCatalogInfo (fileRef, NULL, NULL, NULL, &fileSpec, NULL);
	if (err != noErr)
	{
		fprintf(stderr, "FSGetCatalogInfo(): Error %d getting file spec\n", err);
		return err;
	}

    ///////////// oK, now we can go about getting the comment /////////////

	dt.ioVRefNum = fileSpec.vRefNum;
        
    err = PBDTGetPath(&dt);
	if (err != noErr)
	{
		fprintf(stderr, "PBDTGetPath(): Error %d getting OS9 comment\n", err);
		return err;
	}
    
    //fill in the relevant fields (using parameters)
    dt.ioNamePtr = fileSpec.name;
    dt.ioDirID = fileSpec.parID;
    dt.ioDTBuffer = (char *)&buf;
	
	PBDTGetCommentSync(&dt);
	
	if (dt.ioDTActCount != 0) //if zero, that means no comment
	{
		strncpy((char *)&comment, (char *)&buf, dt.ioDTActCount);
		printf("%s\n", (char *)&comment);
	}
	return noErr;
}




#pragma mark -

Boolean MyAEIdleCallback (
   EventRecord * theEvent,
   SInt32 * sleepTime,
   RgnHandle * mouseRgn)
{

	return 0;
}

pascal OSErr MoreFEGetComment(const FSSpecPtr pFSSpecPtr,Str255 pCommentStr,const AEIdleUPP pIdleProcUPP)
{
  AppleEvent tAppleEvent = {typeNull,NULL};  //  If you always init AEDescs, it's always safe to dispose of them.
  AEDesc tAEDesc = {typeNull,NULL};
  OSErr anErr = noErr;

  if (NULL == pIdleProcUPP)  // the idle proc is required
  {
	fprintf(stderr, "No proc pointer\n");
    return paramErr;
  }
  anErr = MoreAEOCreateObjSpecifierFromFSSpec(pFSSpecPtr,&tAEDesc);
  if (anErr)
  {
	fprintf(stderr, "Error creating objspecifier from fsspec\n");
    return paramErr;
  }
  if (noErr == anErr)
  {
    AEBuildError  tAEBuildError;

    anErr = AEBuildAppleEvent(
              kAECoreSuite,kAEGetData,
          typeApplSignature,&gFinderSignature,sizeof(OSType),
              kAutoGenerateReturnID,kAnyTransactionID,
              &tAppleEvent,&tAEBuildError,
              "'----':obj {form:prop,want:type(prop),seld:type(comt),from:(@)}",&tAEDesc);

        // always dispose of AEDescs when you are finished with them
        (void) MoreAEDisposeDesc(&tAEDesc);

    if (noErr == anErr)
    {  
      //  Send the event.
      anErr = MoreAESendEventReturnPString(pIdleProcUPP,&tAppleEvent,pCommentStr);
	  if (anErr)
	  {
		fprintf(stderr, "Error sending event to get pascal string\n");
	  }
      // always dispose of AEDescs when you are finished with them
      (void) MoreAEDisposeDesc(&tAppleEvent);
    }
	else
	{
		fprintf(stderr, "Error building Apple Event\n");
	}
  }
  return anErr;
}  // end MoreFEGetComment

/********************************************************************************
  Send an Apple event to the Finder to get the finder comment of the item 
  specified by the FSRefPtr.

  pFSRefPtr    ==>    The item to get the file kind of.
  pCommentStr    ==>    A string into which the finder comment will be returned.
  pIdleProcUPP  ==>    A UPP for an idle function (required)
  
  See note about idle functions above.
*/
#if TARGET_API_MAC_CARBON
pascal OSErr MoreFEGetCommentCFString(const FSRefPtr pFSRefPtr, CFStringRef* pCommentStr,const AEIdleUPP pIdleProcUPP)
{
  AppleEvent tAppleEvent = {typeNull,NULL};  //  If you always init AEDescs, it's always safe to dispose of them.
  AEDesc tAEDesc = {typeNull,NULL};
  OSErr anErr = noErr;

  if (NULL == pIdleProcUPP)  // the idle proc is required
    return paramErr;


  anErr = MoreAEOCreateObjSpecifierFromFSRef(pFSRefPtr,&tAEDesc);

  if (noErr == anErr)
  {
    AEBuildError  tAEBuildError;

    anErr = AEBuildAppleEvent(
              kAECoreSuite,kAEGetData,
          typeApplSignature,&gFinderSignature,sizeof(OSType),
              kAutoGenerateReturnID,kAnyTransactionID,
              &tAppleEvent,&tAEBuildError,
              "'----':obj {form:prop,want:type(prop),seld:type(comt),from:(@)}",&tAEDesc);

        // always dispose of AEDescs when you are finished with them
        (void) MoreAEDisposeDesc(&tAEDesc);

    if (noErr == anErr)
    {  
#if 0  // Set this true to printf the Apple Event before you send it.
      Handle strHdl;
      anErr = AEPrintDescToHandle(&tAppleEvent,&strHdl);
      if (noErr == anErr)
      {
        char  nul  = '\0';
        PtrAndHand(&nul,strHdl,1);
        printf("\n-MoreFEGetCommentCFString: tAppleEvent=%s.",*strHdl); fflush(stdout);
        DisposeHandle(strHdl);
      }
#endif
      //  Send the event.
      anErr = MoreAESendEventReturnAEDesc(pIdleProcUPP,&tAppleEvent,typeUnicodeText,&tAEDesc);
      // always dispose of AEDescs when you are finished with them
      (void) MoreAEDisposeDesc(&tAppleEvent);
      if (noErr == anErr)
      {
        anErr = MoreAEGetCFStringFromDescriptor(&tAEDesc,pCommentStr);
        // always dispose of AEDescs when you are finished with them
        (void) MoreAEDisposeDesc(&tAEDesc);
      }
    }
  }
  return anErr;
}  // end MoreFEGetCommentCFString
#endif


//*******************************************************************************
// Disposes of desc and initialises it to the null descriptor.
pascal void MoreAEDisposeDesc(AEDesc* desc)
{
	OSStatus junk;

	MoreAssertQ(desc != nil);
	
	junk = AEDisposeDesc(desc);
	MoreAssertQ(junk == noErr);

	MoreAENullDesc(desc);
}//end MoreAEDisposeDesc

//*******************************************************************************
// Initialises desc to the null descriptor (typeNull, nil).
pascal void MoreAENullDesc(AEDesc* desc)
{
	MoreAssertQ(desc != nil);

	desc->descriptorType = typeNull;
	desc->dataHandle     = nil;
}//end MoreAENullDesc

//********************************************************************************
// A simple wrapper around CreateObjSpecifier which creates
// an object specifier from a FSSpec using formName.
pascal OSStatus MoreAEOCreateObjSpecifierFromFSSpec(const FSSpecPtr pFSSpecPtr,AEDesc *pObjSpecifier)
{
	OSErr 		anErr = paramErr;

	if (nil != pFSSpecPtr)
	{
		FSRef tFSRef;

		anErr = FSpMakeFSRef(pFSSpecPtr,&tFSRef);
		if (noErr == anErr)
		{
			anErr = MoreAEOCreateObjSpecifierFromFSRef(&tFSRef,pObjSpecifier);
		}
	}
	return anErr;
}//end MoreAEOCreateObjSpecifierFromFSSpec



//********************************************************************************
// A simple wrapper around CreateObjSpecifier which creates
// an object specifier from a FSRef and using formName.
pascal OSStatus MoreAEOCreateObjSpecifierFromFSRef(const FSRefPtr pFSRefPtr,AEDesc *pObjSpecifier)
{
	OSErr 		anErr = paramErr;

	if (nil != pFSRefPtr)
	{
		CFURLRef tCFURLRef = CFURLCreateFromFSRef(kCFAllocatorDefault,pFSRefPtr);

		if (nil != tCFURLRef)
		{
			anErr = MoreAEOCreateObjSpecifierFromCFURLRef(tCFURLRef,pObjSpecifier);
			CFRelease(tCFURLRef);
		}
		else
			anErr = coreFoundationUnknownErr;
	}
	return anErr;
}



//********************************************************************************
// A simple wrapper around CreateObjSpecifier which creates
// an object specifier from a CFURLRef and using formName.

pascal OSStatus MoreAEOCreateObjSpecifierFromCFURLRef(const CFURLRef pCFURLRef,AEDesc *pObjSpecifier)
{
	OSErr 		anErr = paramErr;

	if (nil != pCFURLRef)
	{
		Boolean 		isDirectory = CFURLHasDirectoryPath(pCFURLRef);
		CFStringRef		tCFStringRef = CFURLCopyFileSystemPath(pCFURLRef, kCFURLHFSPathStyle);
		AEDesc 			containerDesc = {typeNull, NULL};
		AEDesc 			nameDesc = {typeNull, NULL};
		UniCharPtr		buf = nil;

		if (nil != tCFStringRef)
		{
			Size	bufSize = (CFStringGetLength(tCFStringRef) + (isDirectory ? 1 : 0)) * sizeof(UniChar);

			buf = (UniCharPtr) NewPtr(bufSize);

			if ((anErr = MemError()) == noErr)
			{
				CFStringGetCharacters(tCFStringRef, CFRangeMake(0,bufSize/2), buf);
				if (isDirectory) (buf)[(bufSize-1)/2] = (UniChar) 0x003A;				
			}
		} else
			anErr = coreFoundationUnknownErr;
		
		if (anErr == noErr)
			anErr = AECreateDesc(typeUnicodeText, buf, GetPtrSize((Ptr) buf), &nameDesc);
		if (anErr == noErr)
			anErr = CreateObjSpecifier(isDirectory ? cFolder : cFile,&containerDesc,formName,&nameDesc,false,pObjSpecifier);

		MoreAEDisposeDesc(&nameDesc);

		if (buf)
			DisposePtr((Ptr)buf);
	}
	return anErr;
}//end MoreAEOCreateObjSpecifierFromCFURLRef

pascal OSStatus MoreAESendEventReturnAEDesc(
            const AEIdleUPP    pIdleProcUPP,
            const AppleEvent  *pAppleEvent,
            const DescType    pDescType,
            AEDesc        *pAEDesc)
{
  OSStatus anError = noErr;

  //  No idle function is an error, since we are expected to return a value
  if (pIdleProcUPP == NULL)
    anError = paramErr;
  else
  {
    AppleEvent theReply = {typeNull,NULL};
    AESendMode sendMode = kAEWaitReply;

    anError = AESend(pAppleEvent, &theReply, sendMode, kAENormalPriority, kNoTimeOut, pIdleProcUPP, NULL);
    //  [ Don't dispose of the event, it's not ours ]
    if (noErr == anError)
    {
      anError = MoreAEGetHandlerError(&theReply);

      if (!anError && theReply.descriptorType != typeNull)
      {
        anError = AEGetParamDesc(&theReply, keyDirectObject, pDescType, pAEDesc);
      }
      MoreAEDisposeDesc(&theReply);
    }
  }
  return anError;
}  // MoreAESendEventReturnAEDesc

pascal OSStatus MoreAESendEventReturnPString(
            const AEIdleUPP pIdleProcUPP,
            const AppleEvent* pAppleEvent,
            Str255 pStr255)
{
  DescType      actualType;
  Size         actualSize;
  OSStatus      anError;

  anError = MoreAESendEventReturnData(pIdleProcUPP,pAppleEvent,typePString,
        &actualType,pStr255,sizeof(Str255),&actualSize);

  if (errAECoercionFail == anError)
  {
    anError =  MoreAESendEventReturnData(pIdleProcUPP,pAppleEvent,typeChar,
      &actualType,(Ptr) &pStr255[1],sizeof(Str255),&actualSize);
    if (actualSize < 256)
      pStr255[0] = (UInt8) actualSize;
    else
      anError = errAECoercionFail;
  }
  return anError;
}  // MoreAESendEventReturnPString


pascal  OSStatus  MoreAEGetHandlerError(const AppleEvent* pAEReply)
{
  OSStatus  anError = noErr;
  OSErr    handlerErr;
  
  DescType  actualType;
  long    actualSize;
  
  if ( pAEReply->descriptorType != typeNull )  // there's a reply, so there may be an error
  {
    OSErr  getErrErr = noErr;
    
    getErrErr = AEGetParamPtr( pAEReply, keyErrorNumber, typeShortInteger, &actualType,
                  &handlerErr, sizeof( OSErr ), &actualSize );
    
    if ( getErrErr != errAEDescNotFound )  // found an errorNumber parameter
    {
      anError = handlerErr;          // so return it's value
    }
  }
  return anError;
}//end MoreAEGetHandlerError

pascal OSStatus MoreAESendEventReturnData(
            const AEIdleUPP    pIdleProcUPP,
            const AppleEvent  *pAppleEvent,
            DescType      pDesiredType,
            DescType*      pActualType,
            void*         pDataPtr,
            Size        pMaximumSize,
            Size         *pActualSize)
{
  OSStatus anError = noErr;

  //  No idle function is an error, since we are expected to return a value
  if (pIdleProcUPP == NULL)
    anError = paramErr;
  else
  {
    AppleEvent theReply = {typeNull,NULL};
    AESendMode sendMode = kAEWaitReply;

    anError = AESend(pAppleEvent, &theReply, sendMode, kAENormalPriority, kNoTimeOut, pIdleProcUPP, NULL);
    //  [ Don't dispose of the event, it's not ours ]
    if (noErr == anError)
    {
      anError = MoreAEGetHandlerError(&theReply);

      if (!anError && theReply.descriptorType != typeNull)
      {
        anError = AEGetParamPtr(&theReply, keyDirectObject, pDesiredType,
              pActualType, pDataPtr, pMaximumSize, pActualSize);
      }
      MoreAEDisposeDesc(&theReply);
    }
  }
  return anError;
}  // MoreAESendEventReturnData


pascal OSErr MoreAEGetCFStringFromDescriptor(const AEDesc* pAEDesc, CFStringRef* pCFStringRef)
{
  AEDesc    uniAEDesc = {typeNull, NULL};
  OSErr    anErr;

  if (NULL == pCFStringRef)
    return paramErr;

  anErr = AECoerceDesc(pAEDesc, typeUnicodeText, &uniAEDesc);
  if (noErr == anErr)
  {
    if (typeUnicodeText == uniAEDesc.descriptorType)
    {
          Size bufSize = AEGetDescDataSize(&uniAEDesc);
          Ptr buf = NewPtr(bufSize);

          if ((noErr == (anErr = MemError())) && (NULL != buf))
          {
              anErr = AEGetDescData(&uniAEDesc, buf, bufSize);
              if (noErr == anErr)
                  *pCFStringRef = CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*) buf, bufSize / (Size) sizeof(UniChar));

              DisposePtr(buf);
          }
    }
    MoreAEDisposeDesc(&uniAEDesc);
  }
  return (anErr);
}//end MoreAEGetCFStringFromDescriptor

