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

/*
  CHANGES

  0.2
    * Updated to use exit code constants from sysexits.h, errors go to stderr

  0.1
    * First release of setlabel
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>
#include <string.h>
#include <sysexits.h>

#define PROGRAM_STRING "setlabel"
#define VERSION_STRING "0.2"
#define AUTHOR_STRING  "Sveinbjorn Thordarson <sveinbt@hi.is>"

static void SetFileLabel (char *path, short labelNum);
static short ParseLabelName (char *labelName);
static short GetLabelNumber (short flags);
static void PrintVersion (void);
static void PrintHelp (void);
static int UnixIsFolder (char *path);
static void SetLabelInFlags (short *flags, short labelNum);

const char labelNames[8][7] = { "None", "Red", "Orange", "Yellow", "Green", "Blue", "Purple", "Gray" };
short silentMode = 0;

static OSErr FSGetDInfo(const FSRef* ref, DInfo *dInfo)
{
  FSCatalogInfo cinfo;
  OSErr err = FSGetCatalogInfo(ref, kFSCatInfoFinderInfo, &cinfo, NULL, NULL, NULL);

  if (err != noErr)
    return err;

  *dInfo = *(DInfo*)cinfo.finderInfo;

  return err;
}

static OSErr FSGetFInfo(const FSRef* ref, FInfo *fInfo)
{
  FSCatalogInfo cinfo;
  OSErr err = FSGetCatalogInfo(ref, kFSCatInfoFinderInfo, &cinfo, NULL, NULL, NULL);

  if (err != noErr)
      return err;

  *fInfo = *(FInfo*)cinfo.finderInfo;

  return err;
}

static OSErr FSSetFInfo(const FSRef *ref, const FInfo *finfo) {
  FSCatalogInfo cinfo;
  *(FInfo*)cinfo.finderInfo = *finfo;

  OSErr err = FSSetCatalogInfo(ref, kFSCatInfoFinderInfo, &cinfo);
  return err;
}

static OSErr FSSetDInfo(const FSRef *ref, const DInfo *dinfo) {
  FSCatalogInfo cinfo;
  *(DInfo*)cinfo.finderInfo = *dinfo;

  OSErr err = FSSetCatalogInfo(ref, kFSCatInfoFinderInfo, &cinfo);
  return err;
}

int main (int argc, const char * argv[])
{
  int   optch;
  static char optstring[] = "vhs";
  char  *labelName;
  short labelNum;

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
      case 's':
        silentMode = 1;
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

///////////////////////////////////////////////////////////////////
// Make sure file exists and we have privileges.  Then set the
// file Finder comment.
///////////////////////////////////////////////////////////////////
static void SetFileLabel (char *path, short labelNum)
{
  OSErr  err = noErr;
  FSRef  fileRef;
  FSSpec fileSpec;
  short  isFldr;
  short  currentLabel;
  FInfo  finderInfo;

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
    DInfo dInfo;
    err = FSGetDInfo (&fileRef, &dInfo);

    if (err != noErr) {
      fprintf(stderr, "Error %d getting file Finder Directory Info\n", err);

      return;
    }

    //get current label
    currentLabel = GetLabelNumber(dInfo.frFlags);

    //set new label into record
    SetLabelInFlags(&dInfo.frFlags, labelNum);

    err = FSSetDInfo(&fileRef, &dInfo);
    if (err != noErr) {
      fprintf(stderr, "Error %d setting file Finder Directory Info\n", err);
      return;
    }
  }

  ///////////////////////// IF SPECIFIED FILE IS A REGULAR FILE /////////////////////////

  else
  {
    /* get the finder info */
    err = FSGetFInfo (&fileRef, &finderInfo);
    if (err != noErr)
      if (!silentMode)
        fprintf(stderr, "FSpGetFInfo(): Error %d getting file Finder info from file spec for file %s", err, path);

    //retrieve the label number of the file
    currentLabel = GetLabelNumber(finderInfo.fdFlags);

    //if it's already set with the desired label we return
    if (currentLabel == labelNum)
      return;

    //set the appropriate value in the flags field
    SetLabelInFlags(&finderInfo.fdFlags, labelNum);

    //apply the settings to the file
    err = FSSetFInfo (&fileRef, &finderInfo);
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

  //
  // nullify former label
  //

  /* Orange */
  if (myFlags & 2 && myFlags & 8 && myFlags & 4)
  {
    myFlags -= 2;
    myFlags -= 8;
    myFlags -= 4;
  }

  /* Red */
  if (myFlags & 8 && myFlags & 4)
  {
    myFlags -= 8;
    myFlags -= 4;
  }

  /* Yellow */
  if (myFlags & 8 && myFlags & 2)
  {
    myFlags -= 8;
    myFlags -= 2;
  }

  /* Blue */
  if (myFlags & 8)
  {
    myFlags -= 8;
  }

  /* Purple */
  if (myFlags & 2 && myFlags & 4)
  {
    myFlags -= 2;
    myFlags -= 4;
  }

  /* Green */
  if (myFlags & 4)
  {
    myFlags -= 4;
  }

  /* Gray */
  if (myFlags & 2)
  {
    myFlags -= 2;
  }

  //
  // Now all the labels should be at off
  // Create flags with the desired label
  //

  switch(labelNum)
  {
    case 0:
      // None
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
  }

  // Set the desired label
  *flags = myFlags;
}

static void PrintVersion (void)
{
  printf("%s version %s by %s\n", PROGRAM_STRING, VERSION_STRING, AUTHOR_STRING);
}

static void PrintHelp (void)
{
  printf("usage: %s [-vhs] label file ...\n", PROGRAM_STRING);
}

/*//////////////////////////////////////
// Checks bits 1-3 of fdFlags and frFlags
// values in FInfo and DInfo structs
// and gets the relevant color/number
/////////////////////////////////////*/
static short GetLabelNumber (short flags)
{
  /* Orange */
  if (flags & 2 && flags & 8 && flags & 4)
    return 2;

  /* Red */
  if (flags & 8 && flags & 4)
    return 1;

  /* Yellow */
  if (flags & 8 && flags & 2)
    return 3;

  /* Blue */
  if (flags & 8)
    return 5;

  /* Purple */
  if (flags & 2 && flags & 4)
    return 6;

  /* Green */
  if (flags & 4)
    return 4;

  /* Gray */
  if (flags & 2)
    return 7;

  return 0;
}

static short ParseLabelName (char *labelName)
{
  short labelNum, i;

  for (i = 0; i < 8; i++)
    if (!strcmp(labelName, (char *)labelNames[i]))
      labelNum = i;

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

  return S_ISREG(filestat.st_mode) != 1;
}
