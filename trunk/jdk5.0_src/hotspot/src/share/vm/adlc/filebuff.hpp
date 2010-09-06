#ifdef USE_PRAGMA_IDENT_HDR
#pragma ident "@(#)filebuff.hpp	1.20 03/12/23 16:38:49 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

// FILEBUFF.HPP - Definitions for parser file buffering routines

#include <iostream.h>

// STRUCTURE FOR HANDLING INPUT AND OUTPUT FILES
typedef struct {
  const char *_name;
  FILE *_fp;
} BufferedFile;

class ArchDesc;

//------------------------------FileBuff--------------------------------------
// This class defines a nicely behaved buffer of text.  Entire file of text
// is read into buffer at creation, with sentinals at start and end.
class FileBuff {
  friend class FileBuffRegion;
 private:
  long  _bufferSize;	        // Size of text holding buffer.
  long  _offset;   		// Expected filepointer offset.
  long  _bufoff; 	        // Start of buffer file offset

  char *_buf;    		// The buffer itself.
  char *_bigbuf;                // The buffer plus sentinals; actual heap area
  char *_bufmax; 		// A pointer to the buffer end sentinal
  char *_bufeol; 		// A pointer to the last complete line end

  int   _err;                   // Error flag for file seek/read operations
  long  _filepos;               // Current offset from start of file

  ArchDesc& _AD;                // Reference to Architecture Description

  // Error reporting function
  void file_error(int flag, int linenum, const char *fmt, ...);

 public:
  const BufferedFile *_fp;           // File to be buffered

  FileBuff(BufferedFile *fp, ArchDesc& archDesc); // Constructor
  ~FileBuff();                  // Destructor

  // This returns a pointer to the start of the current line in the buffer,
  // and increments bufeol and filepos to point at the end of that line.
  char *get_line(void);

  // This converts a pointer into the buffer to a file offset.	It only works
  // when the pointer is valid (i.e. just obtained from getline()).
  int getoff(const char *s) { return _bufoff+(int)(s-_buf); }
};

//------------------------------FileBuffRegion---------------------------------
// A buffer region is really a region of some file, specified as a linked list
// of offsets and lengths.  These regions can be merged; overlapping regions
// will coalesce.
class FileBuffRegion {
 public:			// Workaround dev-studio friend/private bug
  FileBuffRegion *_next;	// Linked list of regions sorted by offset.
 private:
  FileBuff       *_bfr;		// The Buffer of the file
  int _offset, _length;         // The file area
  int             _sol;		// Start of line where the file area starts
  int             _line;	// First line of region

 public:
  FileBuffRegion(FileBuff*, int sol, int line, int offset, int len);
  ~FileBuffRegion();

  FileBuffRegion *copy();	            // Deep copy
  FileBuffRegion *merge(FileBuffRegion*); // Merge 2 regions; delete input

//  void print(std::ostream&);
//  friend std::ostream& operator<< (std::ostream&, FileBuffRegion&);
  void print(ostream&);
  friend ostream& operator<< (ostream&, FileBuffRegion&);
};
