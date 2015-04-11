/*
  geticon - command line program to get icon from Mac OS X files
  Copyright (C) 2004 Sveinbjorn Thordarson <sveinbt@hi.is>

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

  0.2 - sysexits.h constants used as exit codes
  0.1 - geticon first released
*/

#import <Foundation/Foundation.h>
#import "IconFamily.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sysexits.h>

static int GenerateFileFromIcon (char *src, char *dst);
static int GetFileKindFromString (char *str);
static char* CutSuffix (char *name);
static char* GetFileNameFromPath (char *name);
static void PrintHelp (void);
static void PrintVersion (void);

#define PROGRAM_STRING "geticon"
#define VERSION_STRING "0.2"
#define AUTHOR_STRING  "Sveinbjorn Thordarson"
#define OPT_STRING     "vho:"

int main (int argc, const char * argv[])
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  int         optch, result;
  char        *src = NULL, *dst = NULL;
  int         alloced = TRUE;
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
      case 'o':
        dst = optarg;
        alloced = FALSE;
      }
  }

  src = (char *)argv[optind];

  //check if a correct number of arguments was submitted
  if (argc < 2 || src == NULL)
  {
    fprintf(stderr, "%s: Too few arguments.\n", PROGRAM_STRING);
    PrintHelp();
    return EX_USAGE;
  }

  //make destination icon file path current working directory with filename plus icns suffix
  if (dst == NULL)
  {
    dst = malloc(2048);
    strcpy(dst, src);
    char *name = GetFileNameFromPath(dst);
    memmove(dst, name, strlen(name) + 1);
    dst = CutSuffix(dst);
  }

  result = GenerateFileFromIcon(src, dst);

  if (alloced == TRUE)
    free(dst);

  [pool release];
  return result;
}

static int GenerateFileFromIcon (char *src, char *dst)
{
  NSString     *srcStr = [NSString stringWithCString: src];
  NSString     *dstStr = [NSString stringWithCString: dst];
  NSData       *data;
  NSDictionary *dict;

  //make sure source file we grab icon from exists
  if (![[NSFileManager defaultManager] fileExistsAtPath: srcStr])
  {
    fprintf(stderr, "%s: %s: No such file or directory\n", PROGRAM_STRING, src);
    return EX_NOINPUT;
  }

  IconFamily *icon = [IconFamily iconFamilyWithIconOfFile: srcStr];

  if (![dstStr hasSuffix: @".icns"])
    dstStr = [dstStr stringByAppendingString:@".icns"];

  [icon writeToFile: dstStr];

  //see if file was created
  if (![[NSFileManager defaultManager] fileExistsAtPath: dstStr])
  {
    fprintf(stderr, "%s: %s: File could not be created\n", PROGRAM_STRING, dst);
    return EX_CANTCREAT;
  }

  return EX_OK;
}

////////////////////////////////////////
// Cuts suffix from a file name
///////////////////////////////////////
static char* CutSuffix (char *name)
{
  short i, len, suffixMaxLength = 11;

  len = strlen(name);

  for (i = 1; i < suffixMaxLength+2; i++)
  {
    if (name[len-i] == '.')
    {
      name[len-i] = NULL;
      return name;
    }
  }

  return name;
}

static char* GetFileNameFromPath (char *name)
{
  short i, len;

  len = strlen(name);

  for (i = len; i > 0; i--)
    if (name[i] == '/')
      return (char *)&name[i+1];

  return name;
}

static void PrintVersion (void)
{
  printf("%s version %s by %s\n", PROGRAM_STRING, VERSION_STRING, AUTHOR_STRING);
}

static void PrintHelp (void)
{
  printf("usage: %s [-vh] [-t [icns|png|gif|tiff|jpeg]] [-o outputfile] [file]\n", PROGRAM_STRING);
}
