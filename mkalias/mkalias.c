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

/*
  CHANGES

  0.5
    * Sysexits.h used for return values

  0.4
    * Added support for creating relative aliases (see http://developer.apple.com/technotes/tn/tn1188.html for why this is useful)
    * Fixed bug where mkalias would run without the required arguments and exit with major errors

  0.3
    * Added setting of type and creator code for files (courtesy of Chris Bailey <crb@users.sourceforge.net>)
      * Can be switched off with the the -t flag
    * Changed use of NewAliasMinimal (deprecated) to FSNewAliasMinimal (courtesy of Chris Bailey <crb@users.sourceforge.net>)
    * Fixed ill-formed usage string
    * More informative error messages :)

  0.2
    * Added custom icon copying and the -c flag option to turn it off.
    * Internal methods made static.

  0.1
    * Initial release of mkalias
*/

/*
  TODO

  * Ability to create symlink/alias combos
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

#define PROGRAM_STRING "mkalias"
#define VERSION_STRING "0.5"
#define AUTHOR_STRING  "Sveinbjorn Thordarson"
#define OPT_STRING     "vhctr"

static void CreateAlias (char *srcPath, char *destPath);
static short UnixIsFolder (char *path);
static void PrintVersion (void);
static void PrintHelp (void);

short noCustomIconCopy = false;
short noCopyFileCreatorTypes = false;
short makeRelativeAlias = false;

int main (int argc, const char * argv[])
{
  int     optch;
  static char optstring[] = OPT_STRING;

  while ((optch = getopt(argc, (char * const *)argv, optstring)) != -1)
  {
    switch(optch)
    {
      case 'v':
        PrintVersion();
        return EX_OK;
      case 'h':
        PrintHelp();
        return EX_OK;
      case 'c':
        noCustomIconCopy = true;
        break;
      case 't':
        noCopyFileCreatorTypes = true;
        break;
      case 'r':
        makeRelativeAlias = true;
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

  CreateAlias(/*source*/(char *)argv[optind], /*destination*/(char *)argv[optind+1]);

  return EX_OK;
}

static OSErr FSGetFInfo(const FSRef* ref, FInfo *fInfo) {
  FSCatalogInfo cinfo;
  OSErr err = FSGetCatalogInfo(ref, kFSCatInfoFinderInfo, &cinfo, NULL, NULL, NULL);

  if (err != noErr)
    return err;

  *fInfo = *(FInfo*)cinfo.finderInfo;
  return err;
}

static OSErr FSSetFInfo(const FSRef* ref, FInfo *fInfo) {
  FSCatalogInfo cinfo;
  *(FInfo*)cinfo.finderInfo = *fInfo;

  OSErr err = FSSetCatalogInfo(ref, kFSCatInfoFinderInfo, &cinfo);
  return err;
}

static OSErr myFSCreateResFile(const char *path, OSType creator, OSType fileType, FSRef *outRef) {
  int fd = open(path, O_CREAT | O_WRONLY, 0666);
  if (fd == -1) {
    perror("opening destination:");
    return bdNamErr;
  }
  close(fd);

  FSRef ref;

  OSErr err = FSPathMakeRef((const UInt8*)path, &ref, NULL);
  if (err != noErr)
    return err;

  HFSUniStr255 rname;
  FSGetResourceForkName(&rname);

  err = FSCreateResourceFork(&ref, rname.length, rname.unicode, 0);
  if (err != noErr)
    return err;

  FInfo finfo;

  err = FSGetFInfo(&ref, &finfo);
  if (err != noErr)
    return err;

  finfo.fdCreator = creator;
  finfo.fdType = fileType;

  err = FSSetFInfo(&ref, &finfo);
  if (err != noErr)
      return err;

  *outRef = ref;

  return noErr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Given two path strings, srcPath and destPath, creates a MacOS Finder alias from file in srcPath
//  in the destPath, complete with custom icon and all.  Pretty neat.
//
//////////////////////////////////////////////////////////////////////////////////////////////////
static void CreateAlias (char *srcPath, char *destPath)
{
  OSErr err;

  FSSpec sourceFSSpec;
  FSRef srcRef, destRef;
  OSType srcFileType = (OSType)NULL;
  OSType srcCreatorType = (OSType)NULL;
  FInfo srcFinderInfo, destFinderInfo;

  int fd;
  SInt16 rsrcRefNum;

  IconRef srcIconRef;
  IconFamilyHandle srcIconFamily;
  SInt16 theLabel;

  AliasHandle alias;
  short isSrcFolder;

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

  //retrieve source filespec from source file ref
  err = FSGetCatalogInfo (&srcRef, NULL, NULL, NULL, &sourceFSSpec, NULL);
  if (err != noErr)
  {
    fprintf(stderr, "FSGetCatalogInfo(): Error %d getting file spec from source FSRef\n", err);
    exit(EX_IOERR);
  }

  //get the finder info for the source if it's a folder
  if (!isSrcFolder)
  {
    err = FSGetFInfo (&srcRef, &srcFinderInfo);
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
    err = GetIconRefFromFileInfo(
      &srcRef, 0, NULL, 0, NULL,
      kIconServicesNormalUsageFlag, &srcIconRef, &theLabel
    );

    if (err != noErr)
      fprintf(stderr, "GetIconRefFromFile(): Error getting source file's icon.\n");

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
    err = myFSCreateResFile(destPath, 'TEMP', 'TEMP', &destRef);
    if (err != noErr)
    {
      fprintf(stderr, "FSpCreateResFile(): Error %d while creating file\n", err);
      exit(EX_CANTCREAT);
    }

    //create the alias record, relative to the new alias file
    err = FSNewAlias(&destRef, &srcRef, &alias);
    if (err != noErr)
    {
      fprintf(stderr, "NewAlias(): Error %d while creating relative alias\n", err);
      exit(EX_CANTCREAT);
    }

    // save the resource
    rsrcRefNum = FSOpenResFile(&destRef, fsRdWrPerm);
    if (rsrcRefNum == -1)
    {
      err = ResError();
      fprintf(stderr, "Error %d while opening resource fork for %s\n", err, (char *)&destPath);
      exit(EX_IOERR);
    }

    UseResFile(rsrcRefNum);
    Str255 rname;
    AddResource((Handle) alias, rAliasType, 0, NULL);

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
      fprintf(stderr, "NewAliasMinimal(): Null handle Alias returned from FSRef for file %s\n", srcPath);
      exit(EX_IOERR);
    }

    // Create alias resource file
    // If we're dealing with a folder, we use the Finder types for folder
    // Otherwise, we use the same File/Creator as source file

    if (isSrcFolder)
      err = myFSCreateResFile(destPath, 'MACS', 'fdrp', &destRef);
    else
      err = myFSCreateResFile(destPath, '    ', '    ', &destRef);

    if (err != noErr)
    {
      fprintf(stderr, "Error %d while creating alias file\n", err);
      exit(EX_CANTCREAT);
    }

    //open resource file and write the relevant resources
    rsrcRefNum = FSOpenResFile (&destRef, 3);

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
  err = FSGetFInfo (&destRef, &destFinderInfo);
  if (err != noErr)
  {
    printf("FSpGetFInfo(): Error %d getting Finder info for target alias \"%s\"\n", err, destPath);
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

  err = FSSetFInfo (&destRef, &destFinderInfo);
  if (err != noErr)
  {
    printf("FSpSetFInfo(): Error %d setting Finder Alias flag (0x8000).\n", err);
    exit(EX_IOERR);
  }
}

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

  return S_ISREG(filestat.st_mode) != 1;
}

static void PrintVersion (void)
{
  printf("%s version %s by %s\n", PROGRAM_STRING, VERSION_STRING, AUTHOR_STRING);
}

static void PrintHelp (void)
{
  printf("usage: %s [-%s] [source-file] [target-alias]\n", PROGRAM_STRING, OPT_STRING);
}
