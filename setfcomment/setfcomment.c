/*
    setfcomment - Set the Finder comment of file(s)
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
    
	0.2 - New "Silent Mode" flag, exit values from sysexit.h, errors go to stderr
    0.1 - First release of setfcomment

*/

/*  TODO
        
    * Recursively scan directories option
	
*/


/*
    Command line options

    v - version
    h - help - usage
    c [str] - comment string passed as parameter
    n - don't set MacOS 9 comment
    
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <Carbon/Carbon.h>
#include <string.h>
#include <sysexits.h>

///////////////  Definitions    //////////////

#define		MAX_COMMENT_LENGTH	200
#define		PROGRAM_STRING  	"setfcomment"
#define		VERSION_STRING		"0.2"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson"


// Some MoreAppleEvents voodoo
#define MoreAssert(x) (true)
#define MoreAssertQ(x)


/////////////// Prototypes  /////////////////

    // my stuff

    static char *GetCommentParameter (char *arg);
    static void SetFileComment (char *path, char *comment);
    static OSErr OSX_SetComment (FSSpec *fileSpec, char *comment);
    static OSErr OS9_SetComment (FSSpec *fileSpec, char *comment);
    static void PrintVersion (void);
    static void PrintHelp (void);
    
    // the stuff I ripped from MoreAppleEvents sample code

    pascal OSErr MoreFESetComment(const FSSpecPtr pFSSpecPtr, const Str255 pCommentStr, const AEIdleUPP pIdleProcUPP);
    //pascal OSErr MoreFEGetComment(const FSSpecPtr pFSSpecPtr,Str255 pCommentStr,const AEIdleUPP pIdleProcUPP);
    pascal OSStatus MoreAEOCreateObjSpecifierFromFSSpec(const FSSpecPtr pFSSpecPtr,AEDesc *pObjSpecifier);
    pascal OSStatus MoreAEOCreateObjSpecifierFromFSRef(const FSRefPtr pFSRefPtr,AEDesc *pObjSpecifier);
    pascal OSStatus MoreAEOCreateObjSpecifierFromCFURLRef(const CFURLRef pCFURLRef,AEDesc *pObjSpecifier);
    pascal OSErr MoreAESendEventNoReturnValue (const AEIdleUPP pIdleProcUPP, const AppleEvent* pAppleEvent );
    pascal OSErr MoreAEGetHandlerError(const AppleEvent* pAEReply);
    pascal void MoreAEDisposeDesc(AEDesc* desc);
    pascal void MoreAENullDesc(AEDesc* desc);
    
    
/////////////// Globals  /////////////////

static const OSType 	gFinderSignature 	= 'MACS';
static short			setOS9comment 		= 1;
static short			silentMode			= 0;

int main (int argc, const char * argv[]) 
{
    int			rc;
    int			optch;
    static char	optstring[] = "vhsnc:";
    char		*comment = NULL;

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
            case 'c':
                comment = GetCommentParameter(optarg);
                break;
            case 'n':
                setOS9comment = 0;
                break;
            default: // '?'
                rc = 1;
                PrintHelp();
                return EX_USAGE;
        }
    }
    
    if (comment == NULL)
    {
        fprintf(stderr, "Invalid usage: You must specify the comment to set using the -c option.\n");
        return(EX_USAGE);
    }

    //all remaining arguments should be files
    for (; optind < argc; ++optind)
        SetFileComment((char *)argv[optind], comment);

    return EX_OK;
}





#pragma mark -

///////////////////////////////////////////////////////////////////
// Check out the string passed as parameter with the -c option
// Make sure it's within reasonable bounds etc.
///////////////////////////////////////////////////////////////////
static char *GetCommentParameter (char *arg)
{
    static char 	comment[200];
    
    if (strlen(arg) > MAX_COMMENT_LENGTH)
    {
        fprintf(stderr, "Invalid parameter: %s\n", arg);
        fprintf(stderr, "Max comment length is 200 characters\n");
        exit(EX_DATAERR);
    }
    strcpy((char *)&comment, arg);
    return((char *)&comment);
}

///////////////////////////////////////////////////////////////////
// Make sure file exists and we have privileges.  Then set the 
// file Finder comment.
///////////////////////////////////////////////////////////////////
static void SetFileComment (char *path, char *comment)
{
    OSErr	err = noErr;
    FSRef	fileRef;
    FSSpec	fileSpec;

    ///////////// Make sure we can do as we please :) /////////////

            //see if the file in question exists and we can write it
            if (access(path, R_OK|W_OK|F_OK) == -1)
            {
				perror(path);
                return;
            }
            
            //get file reference from path
            err = FSPathMakeRef(path, &fileRef, NULL);
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
    
    
    ///////////// oK, now we can go about setting the comment /////////////
    
    
            //being by setting MacOS X Finder Comment
            err = OSX_SetComment (&fileSpec, comment);
            if (err != noErr)
            {
                fprintf(stderr, "OSX_SetComment(): Error %d setting Finder comment for %s\n", err, path);
                return;
            }
            else if (!silentMode)
			{
                printf("Finder Comment set for %s\n", path);
			}
			
            //check if we're setting OS9 comment.  If not, we bail out here
            if (!setOS9comment)
                return;
    
            //set MacOS 9 Comment
            err = OS9_SetComment (&fileSpec, comment);
            if (err != noErr)
            {
                fprintf(stderr, "OS9_SetComment(): Error %d setting MacOS 9 comment for %s\n", err, path);
                return;
            }
            else if (!silentMode)
			{
                printf("MacOS 9 Comment set for %s\n", path);
			}
}




#pragma mark -



///////////////////////////////////////////////////////////////////
// We set the MacOS X Finder comment by sending an Apple Event to
// the Finder.  As far as I know, there is no other way.
// This will fail if the Finder isn't running....:(
///////////////////////////////////////////////////////////////////
static OSErr OSX_SetComment (FSSpec *fileSpec, char *comment)
{
    OSErr	err = noErr;
    Str255	pCommentStr;

    if (strlen(comment) > MAX_COMMENT_LENGTH)
        return true;

    CopyCStringToPascal(comment,pCommentStr);

    err = MoreFESetComment(fileSpec, pCommentStr, NULL);
    
    return(err);
}


///////////////////////////////////////////////////////////////////
// We set the MacOS 9 Finder comment using the File Manager API's
// Desktop Database functions.
///////////////////////////////////////////////////////////////////
static OSErr OS9_SetComment (FSSpec *fileSpec, char *comment)
{
    OSErr	err = noErr;
    
    DTPBRec dt;
        
    dt.ioVRefNum = (*fileSpec).vRefNum;
        
    err = PBDTGetPath(&dt);
    
    //fill in the relevant fields (using parameters)
    dt.ioNamePtr = (*fileSpec).name;
    dt.ioDirID = (*fileSpec).parID;
    dt.ioDTBuffer = comment;
    dt.ioDTReqCount = strlen(comment);
    
    if (PBDTSetCommentSync (&dt) != noErr)
            return (err);
    
    err = PBDTSetCommentSync(&dt);
    if (err != noErr)
        return err; 
        
    err = PBDTFlushSync (&dt);
        
    return (noErr);
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
    printf("usage: %s [-vhn] [-c comment] [file ...]\n", PROGRAM_STRING);
}













#pragma mark -
///////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////// The stuff I ripped from MoreAppleEvents sample code //////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//  The rest of this document falls not under the GPL but under Apple's sample code license, which
//  can be got by downloading the MoreAppleEvents sample code package from developer.apple.com
///////////////////////////////////////////////////////////////////////////////////////////////////


/********************************************************************************
	Send an Apple event to the Finder to set the finder comment of the item 
	specified by the FSSpecPtr.

	pFSSpecPtr		==>		The item to set the file kind of.
	pCommentStr		==>		A string to which the file comment will be set
	pIdleProcUPP	==>		A UPP for an idle function, or nil.
	
	See note about idle functions above.
*/
pascal OSErr MoreFESetComment(const FSSpecPtr pFSSpecPtr, const Str255 pCommentStr, const AEIdleUPP pIdleProcUPP)
{
	AppleEvent tAppleEvent = {typeNull,nil};	//	If you always init AEDescs, it's always safe to dispose of them.
	AEBuildError	tAEBuildError;
	AEDesc 			tAEDesc = {typeNull,nil};
	OSErr anErr = noErr;

	anErr = MoreAEOCreateObjSpecifierFromFSSpec(pFSSpecPtr,&tAEDesc);
	if (noErr == anErr)
	{
		char* dataPtr = NewPtr(pCommentStr[0]);

		CopyPascalStringToC(pCommentStr,dataPtr);
		anErr = AEBuildAppleEvent(
			        kAECoreSuite,kAESetData,
					typeApplSignature,&gFinderSignature,sizeof(OSType),
			        kAutoGenerateReturnID,kAnyTransactionID,
			        &tAppleEvent,&tAEBuildError,
			        "'----':obj {form:prop,want:type(prop),seld:type(comt),from:(@)},data:'TEXT'(@)",
			        &tAEDesc,dataPtr);

		DisposePtr(dataPtr);

		if (noErr == anErr)
		{	
			//	Send the event. In this case we don't care about the reply
			anErr = MoreAESendEventNoReturnValue(pIdleProcUPP,&tAppleEvent);
			(void) MoreAEDisposeDesc(&tAppleEvent);	// always dispose of AEDescs when you are finished with them
		}
	}
	return anErr;
}	// end MoreFESetComment


/********************************************************************************
	Send an Apple event to the Finder to get the finder comment of the item 
	specified by the FSSpecPtr.

	pFSSpecPtr		==>		The item to get the file kind of.
	pCommentStr		==>		A string into which the finder comment will be returned.
	pIdleProcUPP	==>		A UPP for an idle function (required)
	
	See note about idle functions above.
*/
/*pascal OSErr MoreFEGetComment(const FSSpecPtr pFSSpecPtr,Str255 pCommentStr,const AEIdleUPP pIdleProcUPP)
{
	AppleEvent 	tAppleEvent 	= {typeNull,nil};//If you always init AEDescs, it's always safe to dispose of them.
	AEDesc 		tAEDesc 	= {typeNull,nil};
	OSErr anErr = noErr;

	if (nil == pIdleProcUPP)// the idle proc is required
		return paramErr;

	anErr = MoreAEOCreateObjSpecifierFromFSSpec(pFSSpecPtr,&tAEDesc);
        
	if (noErr == anErr)
	{
		AEBuildError	tAEBuildError;

		anErr = AEBuildAppleEvent(	kAECoreSuite,kAEGetData,
                                                typeApplSignature,&gFinderSignature,sizeof(OSType),
                                                kAutoGenerateReturnID,kAnyTransactionID,
                                                &tAppleEvent,&tAEBuildError,
                                                "'----':obj {form:prop,want:type(prop),seld:type(comt),from:(@)}",&tAEDesc);

                (void) MoreAEDisposeDesc(&tAEDesc);	// always dispose of AEDescs when you are finished with them

		if (noErr == anErr)
		{	
			//Send the event.
			anErr = MoreAESendEventReturnPString(pIdleProcUPP,&tAppleEvent,pCommentStr);
			(void) MoreAEDisposeDesc(&tAppleEvent);	// always dispose of AEDescs when you are finished with them
		}
	}
	return anErr;
}	// end MoreFEGetComment
*/
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






pascal	OSErr	MoreAESendEventNoReturnValue (const AEIdleUPP pIdleProcUPP, const AppleEvent* pAppleEvent )
{
	OSErr		anErr = noErr;
	AppleEvent	theReply = {typeNull,nil};
	AESendMode	sendMode;

	if (nil == pIdleProcUPP)
		sendMode = kAENoReply;
	else
		sendMode = kAEWaitReply;

	anErr = AESend( pAppleEvent, &theReply, sendMode, kAENormalPriority, kNoTimeOut, pIdleProcUPP, nil );
	if ((noErr == anErr) && (kAEWaitReply == sendMode))
		anErr = MoreAEGetHandlerError(&theReply);

	MoreAEDisposeDesc( &theReply );
	
	return anErr;
}







/********************************************************************************
	Takes a reply event checks it for any errors that may have been returned
	by the event handler. A simple function, in that it only returns the error
	number. You can often also extract an error string and three other error
	parameters from a reply event.
	
	Also see:
		IM:IAC for details about returned error strings.
		AppleScript developer release notes for info on the other error parameters.
	
	pAEReply	==>		The reply event to be checked.

	RESULT CODES
	____________
	noErr				    0	No error	
	????					??	Pretty much any error, depending on what the
                                                        event handler returns for it's errors.
*/
pascal	OSErr	MoreAEGetHandlerError(const AppleEvent* pAEReply)
{
	OSErr		anErr = noErr;
	OSErr		handlerErr;
	
	DescType	actualType;
	long		actualSize;
	
	if ( pAEReply->descriptorType != typeNull )	// there's a reply, so there may be an error
	{
		OSErr	getErrErr = noErr;
		
		getErrErr = AEGetParamPtr( pAEReply, keyErrorNumber, typeShortInteger, &actualType,
									&handlerErr, sizeof( OSErr ), &actualSize );
		
		if ( getErrErr != errAEDescNotFound )	// found an errorNumber parameter
		{
			anErr = handlerErr;					// so return it's value
		}
	}
	return anErr;
}//end MoreAEGetHandlerError




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

/*


pascal OSErr MoreAESendEventReturnPString(
					const AEIdleUPP pIdleProcUPP,
					const AppleEvent* pAppleEvent,
					Str255 pStr255)
{
	DescType			actualType;
	Size 				actualSize;
	OSErr				anErr;

	anErr = MoreAESendEventReturnData(pIdleProcUPP,pAppleEvent,typePString,
				&actualType,pStr255,sizeof(Str255),&actualSize);

	if (errAECoercionFail == anErr)
	{
		anErr =  MoreAESendEventReturnData(pIdleProcUPP,pAppleEvent,typeChar,
			&actualType,(Ptr) &pStr255[1],sizeof(Str255),&actualSize);
		if (actualSize < 256)
			pStr255[0] = actualSize;
		else
			anErr = errAECoercionFail;
	}
	return anErr;
}	// MoreAESendEventReturnPString




pascal OSErr MoreAESendEventReturnData(
						const AEIdleUPP		pIdleProcUPP,
						const AppleEvent	*pAppleEvent,
						DescType			pDesiredType,
						DescType*			pActualType,
						void*		 		pDataPtr,
						Size				pMaximumSize,
						Size 				*pActualSize)
{
	OSErr anErr = noErr;

	//	No idle function is an error, since we are expected to return a value
	if (pIdleProcUPP == nil)
		anErr = paramErr;
	else
	{
		AppleEvent theReply = {typeNull,nil};
		AESendMode sendMode = kAEWaitReply;

		anErr = AESend(pAppleEvent, &theReply, sendMode, kAENormalPriority, kNoTimeOut, pIdleProcUPP, nil);
		//	[ Don't dispose of the event, it's not ours ]
		if (noErr == anErr)
		{
			anErr = MoreAEGetHandlerError(&theReply);

			if (!anErr && theReply.descriptorType != typeNull)
			{
				anErr = AEGetParamPtr(&theReply, keyDirectObject, pDesiredType,
							pActualType, pDataPtr, pMaximumSize, pActualSize);
			}
			MoreAEDisposeDesc(&theReply);
		}
	}
	return anErr;
}	// MoreAESendEventReturnData



*/