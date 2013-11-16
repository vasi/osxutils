// IconFamily.h
// IconFamily class interface
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
// useful.  However, it appears to work fine on Mac OS X 10.1.4. :-)

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

// This class is a Cocoa/Objective-C wrapper for the Mac OS X Carbon API's
// "icon family" data type.  Its main purpose is to enable Cocoa applications
// to easily create custom file icons from NSImage instances, and thus take
// advantage of Mac OS X's new 128x128 RGBA "thumbnail" icon format to provide
// richly detailed thumbnail previews of the files' contents.
//
// Using IconFamily, this becomes as simple as:
//
//      id iconFamily = [IconFamily iconFamilyWithThumbnailsOfImage:anImage];
//      [iconFamily setAsCustomIconForFile:anExistingFile];
//
// You can also write an icon family to an .icns file using the -writeToFile:
// method.

@interface IconFamily : NSObject
{
    IconFamilyHandle hIconFamily;
}

// Convenience methods.  These use the corresponding -init methods to return
// an autoreleased IconFamily instance.
//
// NOTE: +iconFamily relies on -init, which is currently broken (see -init).

+ (IconFamily*) iconFamily;
+ (IconFamily*) iconFamilyWithContentsOfFile:(NSString*)path;
+ (IconFamily*) iconFamilyWithIconOfFile:(NSString*)path;

// Initializes as a new, empty IconFamily.  This is IconFamily's designated
// initializer method.
//
// NOTE: This method is broken until we figure out how to create a valid new
//       IconFamilyHandle!  In the meantime, use -initWithContentsOfFile: to
//       load an existing .icns file that you can use as a starting point, and
//       use -setIconFamilyElement:fromBitmapImageRep: to replace its
//	 elements.  This is what the "MakeThumbnail" demo app does.

- init;

// Initializes an IconFamily by loading the contents of an .icns file.

- initWithContentsOfFile:(NSString*)path;

// Initializes an IconFamily by loading the Finder icon that's assigned to a
// file.

- initWithIconOfFile:(NSString*)path;

// Writes the icon family to an .icns file.

- (BOOL) writeToFile:(NSString*)path;

// Writes the icon family to the resource fork of the specified file as its
// kCustomIconResource, and sets the necessary Finder bits so the icon will
// be displayed for the file in Finder views.

- (BOOL) setAsCustomIconForFile:(NSString*)path;

// Same as the -setAsCustomIconForFile:... methods, but for folders (directories).

- (BOOL) setAsCustomIconForDirectory:(NSString*)path;

// Removes the custom icon (if any) from the specified file's resource fork,
// and clears the necessary Finder bits for the file.  (Note that this is a
// class method, so you don't need an instance of IconFamily to invoke it.)

+ (BOOL) removeCustomIconFromFile:(NSString*)path;

@end
