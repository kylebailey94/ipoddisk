//  Command line tool takes one argument, the path to the directory the Finder will open upon disk mount:
//  SetOpenWindow path/to/directory/to/open

#include <CoreServices/CoreServices.h>

int  main( int argc, const char *argv[] )
{
  OSStatus    err;
  FSRef      dirFSRef;
  FSCatalogInfo  fsCatalogInfo;
  FSVolumeInfo  fsVolumeInfo;

  //  Create an FSRef to the passed in folder we want to automatically open on disk mount
  err  = FSPathMakeRef( (const UInt8 *)argv[1], &dirFSRef, NULL );
  if ( err != noErr )  goto Bail;
  err  = FSGetCatalogInfo( &dirFSRef, kFSCatInfoGettableInfo, &fsCatalogInfo, NULL, NULL, NULL );
  if ( err != noErr )  goto Bail;

  //  In traditional Mac OS, frOpenChain is a list of windows to open. We zero it out for completeness
  ((ExtendedFolderInfo*)&fsCatalogInfo.extFinderInfo)->reserved1  = 0;
  err  = FSSetCatalogInfo( &dirFSRef, kFSCatInfoFinderXInfo, &fsCatalogInfo );
  if ( err != noErr )  goto Bail;

  //  Get the volume finderInfo
  err  = FSGetVolumeInfo( fsCatalogInfo.volume, 0, NULL, kFSVolInfoFinderInfo, &fsVolumeInfo, NULL, NULL );
  if ( err != noErr )  goto Bail;

  //  Set the new folder ID
  ((UInt32*)&(fsVolumeInfo.finderInfo))[2]  = fsCatalogInfo.nodeID;
  err  = FSSetVolumeInfo( fsCatalogInfo.volume, kFSVolInfoFinderInfo, &fsVolumeInfo );
  if ( err != noErr )  goto Bail;

Bail:
  return( err );
}