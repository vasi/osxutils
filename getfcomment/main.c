/*
    getfcomment - prints the comment meta-data of a Mac OS file
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
    
	0.3 - exit constants used from sysexits.h, manpage updated
	0.2 - Apple Events querying Finder for comment -- now compatible with Mac OS X comments
    0.1 - First release of getfcomment.  It only supports Mac OS 9 Desktop Database comments at the moment

*/

/*  TODO
        
    * Mac OS X comments

*/


/*
    Command line options

    v - version
    h - help - usage
	c - get Mac OS 9 Desktop Database comment instead of querying the Finder via Apple Events
    
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <Carbon/Carbon.h>
#include <string.h>
#include <sysexits.h>


// Some MoreAppleEvents stuff
            #define MoreAssert(x) (true)
            #define MoreAssertQ(x)

///////////////  Definitions    //////////////

#define		MAX_COMMENT_LENGTH	255
#define		PROGRAM_STRING  	"getfcomment"
#define		VERSION_STRING		"0.3"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson"

//globals

short	printFileName = false;
short	os9comment = false;

static const OSType gFinderSignature = 'MACS';

// prototypes

	static void PrintOSXComment (char *path);
	static void PrintFileComment (char *path);
    static void PrintVersion (void);
    static void PrintHelp (void);
	
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



//main 

int main (int argc, const char * argv[]) 
{
	int			rc;
    int			optch;
    static char	optstring[] = "vhpc";

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
			case 'p':
				printFileName = true;
				break;
			case 'c':
				os9comment = true;
				break;
            default: // '?'
                rc = 1;
                PrintHelp();
                return EX_USAGE;
        }
    }
	
    //all remaining arguments should be files
    for (; optind < argc; ++optind)
    {    
		if (os9comment)
			PrintFileComment((char *)argv[optind]);
		else
			PrintOSXComment((char *)argv[optind]);
	}
    return EX_OK;
}

static void PrintOSXComment (char *path)
{
	OSErr	err = noErr;
    FSRef	fileRef;
    FSSpec	fileSpec;
	Str255	comment = "\p";
	char	cStrCmt[255] = "\0";
	AEIdleUPP inIdleProc = NewAEIdleUPP(&MyAEIdleCallback);

    ///////////// Make sure we can do as we please :) /////////////

            //see if the file in question exists and we can write it
            if (access(path, R_OK|F_OK) == -1)
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
    
    
    ///////////// oK, now we can go about getting the comment /////////////

	// call the apple event routine. I'm not going to pretend I understand what's going on
	// in all those horribly kludgy functions, but at least it works.
	err = MoreFEGetComment(&fileSpec, comment, inIdleProc);
	if (err)
	{
		fprintf(stderr, "Error %d getting comment for '%s'\n", err, path);
		if (err == -600)
			fprintf(stderr, "Finder is not running\n", err);//requires Finder to be running
		return;
	}
	//convert pascal string to c string
	strncpy((char *)&cStrCmt, (unsigned char *)&comment+1, comment[0]);
	//if there is no comment, we don't print out anything
	if (!strlen((char *)&cStrCmt))
		return;
	
	//print out the comment
	if (!printFileName)
			printf("%s\n", (char *)&cStrCmt);
		else
			printf("Comment for '%s':\n%s\n", path, (char *)&cStrCmt);
}



static void PrintFileComment (char *path)
{
	OSErr	err = noErr;
    FSRef	fileRef;
    FSSpec	fileSpec;
	DTPBRec dt;
	char	buf[255] = "\0";
	char	comment[255] = "\0";

	//see if the file in question exists and we can write it
	if (access(path, R_OK|F_OK) == -1)
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

    ///////////// oK, now we can go about getting the comment /////////////

	dt.ioVRefNum = fileSpec.vRefNum;
        
    err = PBDTGetPath(&dt);
    
    //fill in the relevant fields (using parameters)
    dt.ioNamePtr = fileSpec.name;
    dt.ioDirID = fileSpec.parID;
    dt.ioDTBuffer = (char *)&buf;
	
	PBDTGetCommentSync(&dt);
	
	if (dt.ioDTActCount != 0) //if zero, that means no comment
	{
		strncpy((char *)&comment, (char *)&buf, dt.ioDTActCount);
		if (!printFileName)
			printf("%s\n", (char *)&comment);
		else
			printf("Comment for '%s':\n%s\n", path, (char *)&comment);
	}
	return;
}


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
    printf("usage: %s [-vhp] [file ...]\n", PROGRAM_STRING);
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
