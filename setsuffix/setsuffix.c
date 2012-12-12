/*
    setsuffix - set file suffices in batches
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
    
	0.2 - Fixed all compiler warnings, removed Carbon dependency, syexits.h for program exit codes
    0.1 - First release of setsuffix

*/

/*  TODO
        
        * Find out how Apple stores the "Hide Extension" file meta-data set in Get info and enable setsuffix to modify it
        * Add suffix matching specification (something like "all files with suffix a get suffix b")
        * Recursively scan through folder hierarchies option (although this can, of course, be done by combining 'setsuffix' with 'find')
        * Suggestions are welcome...
*/


/*
    Command line options

    v - version
    h - help - usage
    s - silent mode
    e - exclude files that already have a suffix
    f - ignore folders
    a - add the specified suffix to file names, regardless of any previous suffices
    r - remove suffices
    
    l [length] - length of suffix to take into account
    x [suffix] - the actual suffix string
    
*/


#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <sysexits.h>
#include <stdlib.h>


/////////////////// Definitions //////////////////

#define		PROGRAM_STRING  	"setsuffix"
#define		VERSION_STRING		"0.2"
#define		AUTHOR_STRING 		"Sveinbjorn Thordarson"

#define		OPT_STRING		"vhl:x:sefar"

#define		DEFAULT_SUFFIX_LEGNTH	4
#define		MAX_SUFFIX_LENGTH	10


/////////////////// Prototypes //////////////////

static void SetSuffix (char *path, char *suffix);
static short GetFileSuffix (char *name);
static char* GetSuffixStringFromArgument (char *arg);
static short GetSuffixMaxLength (char *strArg);
static short UnixIsFolder (char *path);
static void PrintVersion (void);
static void PrintHelp (void);

/////////////////// Globals //////////////////

short		silentMode = 0;
short		suffixMaxLength = DEFAULT_SUFFIX_LEGNTH;
short		excludeFilesWithSuffix = 0;
short		ignoreFolders = 1;
short		justAddTheSuffix = 0;
short		removeSuffix = 0;

////////////////////////////////////////////
// main program function
////////////////////////////////////////////
int main (int argc, const char * argv[])
{
    int				rc;
    int				optch;
    static char		optstring[] = OPT_STRING;
    char			*suffix = NULL;

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
            case 'l':
                suffixMaxLength = GetSuffixMaxLength(optarg);
                break;
            case 'x':
                suffix = GetSuffixStringFromArgument(optarg);
                break;
            case 's':
                silentMode = 1;
                break;
            case 'e':
                excludeFilesWithSuffix = 1;
                break;
            case 'f':
                ignoreFolders = 0;
                break;
                break;
            case 'a':
                justAddTheSuffix = 1;
                break;
            case 'r':
                removeSuffix = 1;
                break;
            default: // '?'
                rc = 1;
                PrintHelp();
                return EX_OK;
        }
    }
    
    if (!removeSuffix && suffix == NULL)
    {
        fprintf(stderr, "You must specify the suffix to be set using the -x option.\n");
        PrintHelp();
        return EX_USAGE;
    }
    
    
    //all remaining arguments should be files
   for (; optind < argc; ++optind)
        SetSuffix((char *)argv[optind], suffix);

    return EX_OK;
}


#pragma mark -

////////////////////////////////////////
// Set the specified file's specified suffix
///////////////////////////////////////
static void SetSuffix (char *path, char *suffix)
{
    char	newName[256];
    int		err;
    short	isFldr;
    short	nameEnd;
    
    isFldr = UnixIsFolder(path);
    
    if (isFldr && ignoreFolders)
		return;

    nameEnd = GetFileSuffix(path);
    
    //file has no suffix
    if (nameEnd == 0)
    {
        //if we're supposed to remove suffix and it doesn't have one...well
        if (removeSuffix)
        {
            if (!silentMode)
            {
                fprintf(stderr, "%s: File has no suffix.\n", path);
            }
            return;
        }
        
        //if we're setting suffixes..
        //we simply add the suffix to the unsuffixed file name
        sprintf((char *)&newName, "%s.%s", path, suffix);
        if (!silentMode)
        {
            printf("%s --> %s\n", path, (char *)&newName);
        }
        err = rename(path, (char *)&newName);
        if (err == -1)
        {
            perror("rename()");
        }
        return;
    }
    
    if (excludeFilesWithSuffix)
    {
        return;
    }
    
    //if we're removing suffix
    if (removeSuffix)
    {
        strcpy((char *)newName, path);
        //terminate c string at the dot
        newName[nameEnd] = (char)NULL;
        //rename file
        err = rename(path, (char *)&newName);
        if (err == -1)
        {
            perror("rename(): ");
            return;
        }
        if (!silentMode)
        {
            printf("%s --> %s\n", (char *)path, (char *)&newName);
        }
        return;
    }
    
    //if file suffix matches the one we're setting
    if (!strcmp(suffix, &path[nameEnd+1]) && !justAddTheSuffix)//increment nameEnd by one to get rid of the dot
    {
        //we do nothing
        return;
    }
    
    //ok, at this point we have a file with a suffix that we want to replace with another suffix
    strcpy((char *)&newName, path);
    if (!justAddTheSuffix)
    {
        newName[nameEnd+1] = (char)NULL;//terminate after dot
    }
    else
    {
        strcat((char *)&newName, (char *)".");
    }
    strcat((char *)&newName, suffix);
    err = rename(path, (char *)&newName);
    if (err == -1)
    {
        perror("rename()");
        return;
    }
    if (!silentMode)
    {
        printf("%s --> %s\n", path, (char *)&newName);
    }
}


#pragma mark -

////////////////////////////////////////
// Returns the suffix of a file name,
// provided it is no longer than 
// the specified maximum suffix length
///////////////////////////////////////

static short GetFileSuffix (char *name)
{
    short	i, len, nameEnd = 0;
    
    len = strlen(name);
    
    for (i = 1; i < suffixMaxLength+2; i++)
    {
        if (name[len-i] == '.')
        {
            nameEnd = (len - i);
            break;
        }
    }
    
    return (nameEnd);
}

////////////////////////////////////////
// Gets the suffix specified as argument
// in the -x option, checks if it is
// valid, copies it and returns it
///////////////////////////////////////

static char* GetSuffixStringFromArgument (char *arg)
{
    static char		suffix[MAX_SUFFIX_LENGTH];
    short		len = strlen(arg);
    
    // Check if suffix string specified is of valid length
    if (len > MAX_SUFFIX_LENGTH)
    {
        fprintf(stderr,"Suffix \"%s\" is too long. setsuffix handles a max suffix length of 10 characters.\n", arg);
        PrintHelp();
        return((char *)EX_DATAERR);
    }
    else if (len > suffixMaxLength)
    {
        
        fprintf(stderr,"Suffix \"%s\" is too long.  Specify a longer max suffix length with the -l option.\n", arg);
        PrintHelp();
        return((char *)EX_DATAERR);
    }
    if (len <= 0)
    {
        fprintf(stderr,"Please specify a suffix of at least 1 character.\n");
        PrintHelp();
        return((char *)EX_USAGE);
    }
    
    //check if use made the error of including the dot '.'
    if (suffix[0] == '.')
    {
        fprintf(stderr,"You don't need to write the preceding dot when specifying suffix.\n");
        PrintHelp();
        return((char *)EX_USAGE);
    }
    
    strcpy((char *)&suffix, arg);
    
    return((char*)&suffix);
}

////////////////////////////////////////
// Takes string given as argument to the -l
// option, checks if it is valid and then
// converts it to an integer.
///////////////////////////////////////

static short GetSuffixMaxLength (char *strArg)
{
    char	str[MAX_SUFFIX_LENGTH+1];
    short	num;
    
    strcpy(str, strArg);
        
    num = atoi(str);
    
    if (num < 1)
    {
        printf("%s: Invalid length specified\n", strArg);
        PrintHelp();
        return(EX_USAGE);
    }
    
    if (num > MAX_SUFFIX_LENGTH)
    {
        printf("%s: Length cannot exceed %d\n", strArg, MAX_SUFFIX_LENGTH);
        PrintHelp();
        return(EX_USAGE);
    }
    
    return num;
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
        return 1;

	return 0;
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
    printf("usage: %s [-vhsefar] [-l length] [-x suffix] [file ...]\n", PROGRAM_STRING);
}

