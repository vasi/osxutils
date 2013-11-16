// IconFamily.m
// IconFamily class implementation
// by Troy Stephens, Thomas Schnitzer, David Remahl, Nathan Day and Ben Haller
// version 0.5.1
//
// Project Home Page:
//   http://homepage.mac.com/troy_stephens/software/objects/IconFamily/
//
// Problems, shortcomings, and uncertainties that I'm aware of are flagged
// with "NOTE:".  Please address bug reports, bug fixes, suggestions, etc.
// to me at troy_stephens@mac.com
//
// This code is provided as-is, with no warranty, in the hope that it will be
// useful.  However, it appears to work fine on Mac OS X 10.1.5 and 10.2. :-)

#import "IconFamily.h"
#import "NSString+CarbonFSSpecCreation.h"

static OSErr GetFSRefFInfo(const FSRef *ref, FInfo *finfo) {
	FSCatalogInfo cinfo;
	OSErr err = FSGetCatalogInfo(ref, kFSCatInfoFinderInfo, &cinfo, NULL, NULL, NULL);
	if (err != noErr)
		return err;
	*finfo = *(FInfo*)cinfo.finderInfo;
}

static OSErr SetFSRefFInfo(const FSRef *ref, const FInfo *finfo) {
	FSCatalogInfo cinfo;
	*(FInfo*)cinfo.finderInfo = *finfo;
	OSErr err = FSSetCatalogInfo(ref, kFSCatInfoFinderInfo, &cinfo);
	return err;
}

@interface IconFamily (Internals)

- (BOOL) addResourceType:(OSType)type asResID:(int)resID;

@end

@implementation IconFamily

+ (IconFamily*) iconFamily
{
    return [[[IconFamily alloc] init] autorelease];
}

+ (IconFamily*) iconFamilyWithContentsOfFile:(NSString*)path
{
    return [[[IconFamily alloc] initWithContentsOfFile:path] autorelease];
}

+ (IconFamily*) iconFamilyWithIconOfFile:(NSString*)path
{
    return [[[IconFamily alloc] initWithIconOfFile:path] autorelease];
}

// This is IconFamily's designated initializer.  It creates a new IconFamily that initially has no elements.
//
// The proper way to do this is to simply allocate a zero-sized handle (not to be confused with an empty handle) and assign it to hIconFamily.  This technique works on Mac OS X 10.2 as well as on 10.0.x and 10.1.x.  Our previous technique of allocating an IconFamily struct with a resourceSize of 0 no longer works as of Mac OS X 10.2.
- init
{
    self = [super init];
    if (self) {
        hIconFamily = (IconFamilyHandle) NewHandle( 0 );
        if (hIconFamily == NULL) {
            [self autorelease];
            return nil;
        }
    }
    return self;
}

- initWithContentsOfFile:(NSString*)path
{
    FSRef fsRef;
    OSErr result;
    
    self = [self init];
    if (self) {
        if (hIconFamily) {
            DisposeHandle( (Handle)hIconFamily );
            hIconFamily = NULL;
        }
		if (![path getFSRef:&fsRef createFileIfNecessary:NO]) {
			[self autorelease];
			return nil;
		}
		result = ReadIconFromFSRef( &fsRef, &hIconFamily );
		if (result != noErr) {
			[self autorelease];
			return nil;
		}
    }
    return self;
}

- initWithIconOfFile:(NSString*)path
{
    IconRef	iconRef;
    OSErr	result;
    SInt16	label;
    FSRef	fileRef;

    self = [self init];
    if (self)
    {
        if (hIconFamily)
        {
            DisposeHandle( (Handle)hIconFamily );
            hIconFamily = NULL;
        }

        if( ![path getFSRef:&fileRef createFileIfNecessary:NO] )
        {
            [self autorelease];
            return nil;
        }

        result = GetIconRefFromFileInfo(
                                    &fileRef, 0, NULL, 0, NULL,
									kIconServicesNormalUsageFlag,
                                    &iconRef,
                                    &label );

        if (result != noErr)
        {
            [self autorelease];
            return nil;
        }

        result = IconRefToIconFamily(
                                     iconRef,
                                     kSelectorAllAvailableData,
                                     &hIconFamily );

        if (result != noErr || !hIconFamily)
        {
            [self autorelease];
            return nil;
        }

        ReleaseIconRef( iconRef );
    }
    return self;
}

- (void) dealloc
{
    DisposeHandle( (Handle)hIconFamily );
    [super dealloc];
}

- (BOOL) setAsCustomIconForFile:(NSString*)path
{
    FSSpec targetFileFSSpec;
    FSRef targetFileFSRef;
    FSRef parentDirectoryFSRef;
    SInt16 file;
    OSErr result;
    FInfo finderInfo;
    Handle hExistingCustomIcon;
    Handle hIconFamilyCopy;
	NSDictionary *fileAttributes;
	OSType existingType = kUnknownType, existingCreator = kUnknownType;
        
    // Get an FSRef and an FSSpec for the target file, and an FSRef for its parent directory that we can use in the FNNotify() call below.
    if (![path getFSRef:&targetFileFSRef createFileIfNecessary:NO])
		return NO;
    result = FSGetCatalogInfo( &targetFileFSRef, kFSCatInfoNone, NULL, NULL, &targetFileFSSpec, &parentDirectoryFSRef );
    if (result != noErr)
        return NO;
	
    // Make sure the file has a resource fork that we can open.
	HFSUniStr255 rname;
	FSGetResourceForkName(&rname);
	result = FSCreateResourceFork(&targetFileFSRef, rname.length, rname.unicode, 0);
    if (!(result == noErr || result == errFSForkExists))
		return NO;
    
    // Open the file's resource fork.
    file = FSOpenResFile( &targetFileFSRef, fsRdWrPerm );
    if (file == -1)
		return NO;
        
    // Make a copy of the icon family data to pass to AddResource().
    // (AddResource() takes ownership of the handle we pass in; after the
    // CloseResFile() call its master pointer will be set to 0xffffffff.
    // We want to keep the icon family data, so we make a copy.)
    // HandToHand() returns the handle of the copy in hIconFamily.
    hIconFamilyCopy = (Handle) hIconFamily;
    result = HandToHand( &hIconFamilyCopy );
    if (result != noErr) {
        CloseResFile( file );
        return NO;
    }
    
    // Remove the file's existing kCustomIconResource of type kIconFamilyType
    // (if any).
    hExistingCustomIcon = GetResource( kIconFamilyType, kCustomIconResource );
    if( hExistingCustomIcon )
        RemoveResource( hExistingCustomIcon );
    
    // Now add our icon family as the file's new custom icon.
    AddResource( (Handle)hIconFamilyCopy, kIconFamilyType,
                 kCustomIconResource, "\p");
    if (ResError() != noErr) {
        CloseResFile( file );
        return NO;
    }
    	
    // Close the file's resource fork, flushing the resource map and new icon
    // data out to disk.
    CloseResFile( file );
    if (ResError() != noErr)
		return NO;
	
    // Now we need to set the file's Finder info so the Finder will know that
    // it has a custom icon.  Start by getting the file's current finder info:
    result = GetFSRefFInfo( &targetFileFSRef, &finderInfo );
    if (result != noErr)
		return NO;
    
    // Set the kHasCustomIcon flag, and clear the kHasBeenInited flag.
    //
    // From Apple's "CustomIcon" code sample:    
    //     "set bit 10 (has custom icon) and unset the inited flag
    //      kHasBeenInited is 0x0100 so the mask will be 0xFEFF:"
    //    finderInfo.fdFlags = 0xFEFF & (finderInfo.fdFlags | kHasCustomIcon ) ;
    finderInfo.fdFlags = (finderInfo.fdFlags | kHasCustomIcon ) & ~kHasBeenInited;
	
    // Now write the Finder info back.
    result = SetFSRefFInfo( &targetFileFSRef, &finderInfo );
    if (result != noErr)
		return NO;
        
    // Notify the system that the directory containing the file has changed, to give Finder the chance to find out about the file's new custom icon.
    result = FNNotify( &parentDirectoryFSRef, kFNDirectoryModifiedMessage, kNilOptions );
    if (result != noErr)
        return NO;
	
    return YES;
}

- (BOOL) setAsCustomIconForDirectory:(NSString*)path
{
    NSFileManager *fm = [NSFileManager defaultManager];
    BOOL isDir;
    BOOL exists;
    NSString *iconrPath = [path stringByAppendingPathComponent:@"Icon\r"];
    FSSpec targetFileFSSpec, targetFolderFSSpec;
    FSRef targetFolderFSRef;
    SInt16 file;
    OSErr result;
    FInfo finderInfo;
    FSCatalogInfo catInfo;
    Handle hExistingCustomIcon;
    Handle hIconFamilyCopy;

    exists = [fm fileExistsAtPath:path isDirectory:&isDir];

    if( !isDir || !exists )
        return NO;

    if( [fm fileExistsAtPath:iconrPath] )
    {
        if( ![fm removeFileAtPath:iconrPath handler:nil] )
            return NO;
    }

    if (![iconrPath getFSSpec:&targetFileFSSpec createFileIfNecessary:YES])
        return NO;

    if( ![path getFSSpec:&targetFolderFSSpec createFileIfNecessary:YES] )
        return NO;

    if( ![path getFSRef:&targetFolderFSRef createFileIfNecessary:NO] )
        return NO;

    // Make sure the file has a resource fork that we can open.
	HFSUniStr255 rname;
	FSGetResourceForkName(&rname);
	result = FSCreateResourceFork(&targetFolderFSRef, rname.length, rname.unicode, 0);
    if (!(result == noErr || result == errFSForkExists))
		return NO;

    // Open the file's resource fork, if it has one.
    file = FSOpenResFile( &targetFolderFSRef, fsRdWrPerm );
    if (file == -1)
        return NO;

    // Make a copy of the icon family data to pass to AddResource().
    // (AddResource() takes ownership of the handle we pass in; after the
    // CloseResFile() call its master pointer will be set to 0xffffffff.
    // We want to keep the icon family data, so we make a copy.)
    // HandToHand() returns the handle of the copy in hIconFamily.
    hIconFamilyCopy = (Handle) hIconFamily;
    result = HandToHand( &hIconFamilyCopy );
    if (result != noErr) {
        CloseResFile( file );
        return NO;
    }

    // Remove the file's existing kCustomIconResource of type kIconFamilyType
    // (if any).
    hExistingCustomIcon = GetResource( kIconFamilyType, kCustomIconResource );
    if( hExistingCustomIcon )
        RemoveResource( hExistingCustomIcon );

    // Now add our icon family as the file's new custom icon.
    AddResource( (Handle)hIconFamilyCopy, kIconFamilyType,
                 kCustomIconResource, "\p");

    if (ResError() != noErr) {
        CloseResFile( file );
        return NO;
    }

    // Close the file's resource fork, flushing the resource map and new icon
    // data out to disk.
    CloseResFile( file );
    if (ResError() != noErr)
        return NO;

    // Make folder icon file invisible
    result = GetFSRefFInfo( &targetFolderFSRef, &finderInfo );
    if (result != noErr)
        return NO;
    finderInfo.fdFlags = (finderInfo.fdFlags | kIsInvisible ) & ~kHasBeenInited;
    // And write info back
    result = SetFSRefFInfo( &targetFolderFSRef, &finderInfo );
    if (result != noErr)
        return NO;

    result = FSGetCatalogInfo( &targetFolderFSRef,
                               kFSCatInfoFinderInfo,
                               &catInfo, nil, nil, nil);
    if( result != noErr )
        return NO;

    ((DInfo*)catInfo.finderInfo)->frFlags = ( ((DInfo*)catInfo.finderInfo)->frFlags | kHasCustomIcon ) & ~kHasBeenInited;

    FSSetCatalogInfo( &targetFolderFSRef,
                      kFSCatInfoFinderInfo,
                      &catInfo);
    if( result != noErr )
        return NO;

    // Notify the system that the target directory has changed, to give Finder the chance to find out about its new custom icon.
    result = FNNotify( &targetFolderFSRef, kFNDirectoryModifiedMessage, kNilOptions );
    if (result != noErr)
        return NO;
	
    return YES;
}

- (BOOL) writeToFile:(NSString*)path
{
    NSData* iconData = NULL;

    HLock((Handle)hIconFamily);
    
    iconData = [NSData dataWithBytes:*hIconFamily length:GetHandleSize((Handle)hIconFamily)];
    [iconData writeToFile:path atomically:NO];

    HUnlock((Handle)hIconFamily);

    return YES;
}

@end

@implementation IconFamily (Internals)

- (BOOL) addResourceType:(OSType)type asResID:(int)resID 
{
    Handle hIconRes = NewHandle(0);
    OSErr err;

    err = GetIconFamilyData( hIconFamily, type, hIconRes );

    if( !GetHandleSize(hIconRes) || err != noErr )
        return NO;

    AddResource( hIconRes, type, resID, "\p" );

    return YES;
}

@end

