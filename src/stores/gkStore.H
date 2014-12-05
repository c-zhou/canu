
#ifndef GKSTORE_H
#define GKSTORE_H

#include "AS_global.H"
#include "memoryMappedFile.H"

#include <vector>

using namespace std;

//  The number of library IIDs we can handle.
//
#define AS_MAX_LIBRARIES_BITS      8
#define AS_MAX_LIBRARIES           ((1 << AS_MAX_LIBRARIES_BITS) - 1)

#define LIBRARY_NAME_SIZE          128
#define LIBRARY_NAME_DEFAULT      "UNDEFINED"

//  The number of read IDs we can handle.  This has more implications for
//  ovlStore than gkpStore.  The default of 27 = 134.2 million reads.
//
#define AS_MAX_READS_BITS          27
#define AS_MAX_READS               ((1 << AS_MAX_READS_BITS) - 1)

//  Be aware that overlapper does not work well at large sizes.  Consensus allocates excessive
//  amounts of memory too.
//
#define AS_MAX_READLEN_BITS        16
#define AS_MAX_READLEN             ((1 << AS_MAX_READLEN_BITS) - 1)

//  This indirectly controls how much sequence can be loaded, in that the store
//  can address 2^32 blocks of this many bytes.  QVs are stored in 5/8 of a byte,
//  so with the default of 64, the store can hold at most
//    2^32 * 64 * 5 / 8 = 171.8 Gbp.
//  With the default 134.2 million reads, that's only 1280 bp/read.
//
#define BLOB_BLOCK_SIZE            1

//  Per-library algorithm options.
//
#define CORRECTION_NONE            0x00
#define CORRECTION_CONSENSUS       0x01
#define CORRECTION_MER             0x02

#define INITIALTRIM_NONE           0x00
#define INITIALTRIM_MER_BASED      0x01  //  runs merTrim
#define INITIALTRIM_FLOW_BASED     0x02  //  rules for 454 reads
#define INITIALTRIM_QUALITY_BASED  0x03  //  rules for sanger reads

#define FINALTRIM_NONE             0x00
#define FINALTRIM_LARGEST_COVERED  0x01  //  largest region covered by good overlaps
#define FINALTRIM_EVIDENCE_BASED   0x02  //  rules for sanger reads
#define FINALTRIM_BEST_EDGE        0x03  //  largest region covered by best overlaps (broken)

//  Sequence data storage methods.  This is the format of the in-core structure.
//  The on-disk structure is decoded automagically.
//
#define GKREAD_TYPE_UNKNOWN    0x00
#define GKREAD_TYPE_SEQ_QLT    0x01
#define GKREAD_TYPE_PACBIO     0x02
#define GKREAD_TYPE_MINION     0x03
#define GKREAD_TYPE_1          0x04
#define GKREAD_TYPE_2          0x05
#define GKREAD_TYPE_3          0x06
#define GKREAD_TYPE_4          0x07
#define GKREAD_TYPE_NUM        8


class gkLibrary {
public:
  gkLibrary() {
    memset(_libraryName, 0, sizeof(char) * LIBRARY_NAME_SIZE);
    strcpy(_libraryName, LIBRARY_NAME_DEFAULT);

    gkLibrary_clearFeatures();
  };

  gkLibrary(char const *name) {
    memset(_libraryName, 0, sizeof(char) * LIBRARY_NAME_SIZE);
    strcpy(_libraryName, name);

    gkLibrary_clearFeatures();
  };

  ~gkLibrary() {
  };

  void gkLibrary_clearFeatures(void) {
    //  DO NOT change defaults without updating gkLibrary_parsePreset().
    _isNonRandom                = 0;

    _trustHomopolymerRuns       = 1;
    _doCorrection               = CORRECTION_NONE;

    _initialTrim                = INITIALTRIM_NONE;
    _finalTrim                  = FINALTRIM_LARGEST_COVERED;
    _removeDuplicateReads       = 0;
    _removeSpurReads            = 1;
    _removeChimericReads        = 1;
    _checkForSubReads           = 0;
  };

public:

  char const    *gkLibrary_libraryName(void)           { return(_libraryName);          };
  uint32         gkLibrary_libraryID(void)             { return(_libraryID);            };

  uint32         gkLibrary_isNonRandom(void)           { return(_isNonRandom);          };
  uint32         gkLibrary_trustHomopolymerRuns(void)  { return(_trustHomopolymerRuns); };
  uint32         gkLibrary_correctBases(void)          { return(_doCorrection);         };
  uint32         gkLibrary_initialTrim(void)           { return(_initialTrim);          };
  uint32         gkLibrary_finalTrim(void)             { return(_finalTrim);            };
  uint32         gkLibrary_removeDuplicateReads(void)  { return(_removeDuplicateReads); };
  uint32         gkLibrary_removeSpurReads(void)       { return(_removeSpurReads);      };
  uint32         gkLibrary_removeChimericReads(void)   { return(_removeChimericReads);  };
  uint32         gkLibrary_checkForSubReads(void)      { return(_checkForSubReads);     };

  uint32         gkLibrary_defaultQV(void)             { return(_defaultQV);            };


  void           gkLibrary_setIsNonRandom(bool f)           { _isNonRandom = f;          };
  void           gkLibrary_setTrustHomopolymerRuns(bool f)  { _trustHomopolymerRuns = f; };
  void           gkLibrary_setCorrectBases(bool f)          { _doCorrection = f;         };
  void           gkLibrary_setInitialTrim(char *t);
  void           gkLibrary_setFinalTrim(char *t);
  void           gkLibrary_setRemoveDuplicateReads(bool f)  { _removeDuplicateReads = f; };
  void           gkLibrary_setRemoveSpurReads(bool f)       { _removeSpurReads = f;      };
  void           gkLibrary_setRemoveChimericReads(bool f)   { _removeChimericReads = f;  };
  void           gkLibrary_setCheckForSubReads(bool f)      { _checkForSubReads = f;     };

  void           gkLibrary_setDefaultQV(double qv)          { _defaultQV = qv;           };

  void           gkLibrary_parsePreset(char *t);


private:
  char           _libraryName[LIBRARY_NAME_SIZE];
  uint32         _libraryID;

  //  If set, reads are from a non-random library and shouldn't count toward coverage stats.
  uint32         _isNonRandom;

  //  If set, reads have issues with homopolymer runs.
  uint32         _trustHomopolymerRuns;

  //  Should reads be corrected?  How?
  uint32         _doCorrection;

  //  Should reads be trimmed based on read data alone?  How?
  uint32         _initialTrim;

  //  Should reads be trimmed based on overlaps?  How?
  uint32         _finalTrim;

  //  Should duplicate reads (based on overlaps) be removed?
  uint32         _removeDuplicateReads;

  //  Should spur reads be cleaned up?  How?
  uint32         _removeSpurReads;

  //  Should chimeric reads be cleaned up?  How?
  uint32         _removeChimericReads;

  //  Should PacBio circular sub-reads be cleaned up?  How?
  uint32         _checkForSubReads;

  //  For reads with no QVs, use this.
  uint32         _defaultQV;

  //  How is this read encoded in core?
  uint32         _type;

  friend class gkStore;
};



class gkRead;

class gkReadData {
public:
  gkReadData() {
    _read = NULL;

    _type = GKREAD_TYPE_UNKNOWN;

    _seq = NULL;
    _qlt = NULL;

    _blobLen = 0;
    _blobMax = 0;
    _blob    = NULL;
  };

  ~gkReadData() {
    delete [] _seq;
    delete [] _qlt;

    if (_type == GKREAD_TYPE_PACBIO) {
      delete [] _pacbio._deletionQV;
      delete [] _pacbio._deletionTag;
      delete [] _pacbio._insertionQV;
      delete [] _pacbio._substitutionQV;
      delete [] _pacbio._mergeQV;
    }

    if (_type == GKREAD_TYPE_MINION) {
      delete [] _nanopore._someQV;
    }
  };

  uint32   type(void)         { return(_type); };

  bool     hasSequence(void)  { return(_seq != NULL); };
  bool     hasQualities(void) { return(_qlt != NULL); };

  char    *sequence(void)     { return(_seq);  };
  char    *qualities(void)    { return(_qlt);  };

private:
  gkRead            *_read;     //  Pointer to the mmap'd read

  uint32             _type;

  char              *_seq;      //  Everyone has sequence
  char              *_qlt;      //    and quality

  uint32             _blobLen;
  uint32             _blobMax;
  uint8             *_blob;     //  And maybe even an encoded blob of data from the store.

  void     encodeBlobChunk(char const *tag, uint32 len, void *dat);

  //  Some reads have more data than others.  This used to be a union, but that
  //  only complicated things with no clear beneift.  Yay, we saved a few bytes on
  //  a structure that we have only a few copies in core at a time.

  struct pacbio {
    char          *_deletionQV;
    char          *_deletionTag;
    char          *_insertionQV;
    char          *_substitutionQV;
    char          *_mergeQV;
  } _pacbio;

  struct nanopore {
    char          *_someQV;
  } _nanopore;


  friend class gkRead;
  friend class gkStore;
};




class gkRead {
public:
  gkRead() {
    _readID     = 0;
    _libraryID  = 0;
    _seqLen     = 0;
    _deleted    = true;
    _flagpad    = 0;

    _clrBgn     = 0;
    _clrEnd     = 0;
    _sizepad    = 0;

    _mPtr       = 0;
    _pID        = 0;
    _pPtr       = 0;
  };

  ~gkRead() {
  };

  uint32    gkRead_readID(void)             { return(_readID);           };

  uint32    gkRead_libraryID(void)          { return(_libraryID);        };

  uint32    gkRead_sequenceLength(void)     { return(_seqLen);           };

  bool      gkRead_isDeleted(void)          { return(_deleted);          };
  void      gkRead_isDeleted(bool deleted)  { _deleted = deleted;        };

  uint32    gkRead_clearRegionBegin(void)   { return(_clrBgn);           };
  uint32    gkRead_clearRegionEnd(void)     { return(_clrEnd);           };
  uint32    gkRead_clearRegionLength(void)  { return(_clrEnd - _clrBgn); };

  void      gkRead_setClearRegion(uint32 bgn, uint32 end) {
    _clrBgn = bgn;
    _clrEnd = end;
  };

public:
  bool        gkRead_loadData(gkReadData *readData, void *blob, bool partitioned);

  gkReadData *gkRead_encodeSeqQlt(char *H, char *S, char *Q);
  gkReadData *gkRead_encodeSeqQlt(char *H, char *S, uint32 qv);
  gkReadData *gkRead_encodePacBio(char *H, char *S, char *Q);
  gkReadData *gkRead_encodeMinION(char *H, char *S, char *Q);

private:
  bool        gkRead_decodeSeqQlt(gkReadData *readData, void *blob);
  bool        gkRead_decodePacBio(gkReadData *readData, void *blob);
  bool        gkRead_decodeMinION(gkReadData *readData, void *blob);

private:
  char       *gkRead_encodeSequence(char *sequence, char *encoded);
  char       *gkRead_decodeSequence(char *encoded,  char *sequence);
  
  char       *gkRead_encodeQuality(char *sequence, char *encoded);
  char       *gkRead_decodeQuality(char *encoded,  char *sequence);

private:
  uint64   _readID       : AS_MAX_READS_BITS;       //  Up to 128 million reads
  uint64   _libraryID    : AS_MAX_LIBRARIES_BITS;   //  Up to 256 libraries
  uint64   _seqLen       : AS_MAX_READLEN_BITS;     //  Up to 128 kilobases
  uint64   _deleted      :  1;                      //  Deleted?
  uint64   _flagpad      : (64 -
                            AS_MAX_READS_BITS -
                            AS_MAX_LIBRARIES_BITS -
                            AS_MAX_READLEN_BITS - 1);

  uint64   _clrBgn       : AS_MAX_READLEN_BITS;
  uint64   _clrEnd       : AS_MAX_READLEN_BITS;
  uint64   _sizepad      : 64 - 2 * AS_MAX_READLEN_BITS;

  uint64   _mPtr         : 32;      //  Pointer to blob of data in master blob file (4e9)
  uint64   _pID          : 10;      //  Partition file id, 0...1024
  uint64   _pPtr         : 22;      //  Pointer to blob of data in partitioned blob file (4e6)

  friend class gkStore;
};





//  gkStoreInfo is saved on disk.
//  gkStore is the in memory structure used to access the data.
//
class gkStoreInfo {
public:
  gkStoreInfo() {
    //                      63 61 33 67 3a 47 4b 50
    gkMagic            = 0x636133673a474b50llu;
    gkVersion          = 0x0000000000000001llu;

    gkLibrarySize      = sizeof(gkLibrary);
    gkReadSize         = sizeof(gkRead);
    gkMaxLibrariesBits = AS_MAX_LIBRARIES_BITS;
    gkLibraryNameSize  = LIBRARY_NAME_SIZE;
    gkMaxReadBits      = AS_MAX_READS_BITS;
    gkMaxReadLenBits   = AS_MAX_READLEN_BITS;
    gkBlobBlockSize    = BLOB_BLOCK_SIZE;

    numLibraries       = 0;
    numReads           = 0;
  };
  ~gkStoreInfo() {
  };

private:
  uint64    gkMagic;
  uint64    gkVersion;

  uint32    gkLibrarySize;      //  Sanity checks that this code can load the data properly.
  uint32    gkReadSize;
  uint32    gkMaxLibrariesBits;
  uint32    gkLibraryNameSize;
  uint32    gkMaxReadBits;
  uint32    gkMaxReadLenBits;
  uint32    gkBlobBlockSize;

  uint32    numLibraries;       //  Counts of types of things we have loaded
  uint32    numReads;

  friend class gkStore;
};



//  The default behavior is to open the store for read only, and to load
//  all the metadata into memory.

typedef enum {
  gkStore_readOnly    = 0x00,  //  Open read only
  gkStore_modify      = 0x01,  //  Open for modification
  gkStore_extend      = 0x02,  //  Open for modification and appending new reads/libraries
  gkStore_partitioned = 0x03,  //  Open read only, a partition
} gkStore_mode;


class gkStore {
public:

  //  Open a full gkpStore.
  gkStore(char const *path, gkStore_mode = gkStore_readOnly, uint32 partID=0);

  //  Open a partitioned store, only reading is supported.
  //gkStore(char const *path, uint32 partID);

  ~gkStore();

  const char  *gkStore_path(void) { return(_storePath); };  //  Returns the path to the store
  const char  *gkStore_name(void) { return(_storeName); };  //  Returns the name, e.g., name.gkpStore

  bool         gkStore_loadPartition(uint32 partID);

  void         gkStore_buildPartitions(uint32 *partitionMap, uint32 maxPartition);

  void         gkStore_delete(void);             //  Deletes the files in the store.
  void         gkStore_deletePartitions(void);   //  Deletes the files for a partition.

  uint32       gkStore_getNumLibraries(void)       { return(_info.numLibraries); }
  uint32       gkStore_getNumReads(void)           { return(_info.numReads); }

  gkLibrary   *gkStore_getLibrary(uint32 id)       { return(&_libraries[id]); }
  gkRead      *gkStore_getRead(uint32 id)          { return(&_reads[id]); }

  gkLibrary   *gkStore_addEmptyLibrary(char const *name);
  gkRead      *gkStore_addEmptyRead(gkLibrary *lib);

  bool         gkStore_loadReadData(gkRead *read,   gkReadData *readData) {
    return(gkStore_loadReadData(read->gkRead_readID(), readData));
  };
  bool         gkStore_loadReadData(uint32  readID, gkReadData *readData);

  void         gkStore_stashReadData(gkRead *read, gkReadData *data);

private:
  gkStoreInfo          _info;  //  All the stuff stored on disk.

  char                 _storePath[FILENAME_MAX];  //  Needed to create files
  char                 _storeName[FILENAME_MAX];  //  Useful for log files in other programs

  //  If these are memory mapped, then multiple processes on the same host can share the
  //  (read-only) data.

  memoryMappedFile    *_librariesMMap;
  uint32               _librariesAlloc;  //  If zero, the mmap is used.
  gkLibrary           *_libraries;

  memoryMappedFile    *_readsMMap;
  uint32               _readsAlloc;      //  If zero, the mmap is used.
  gkRead              *_reads;

  memoryMappedFile    *_blobsMMap;       //  Either the full blobs, or the partitioned blobs.
  void                *_blobs;
  FILE                *_blobsFile;

  //  If the store is openend partitioned, this data is loaded from disk

  uint32               _numberOfPartitions;   //  Total number of partitions that exist
  uint32               _partitionID;          //  Which partition this is
  uint32              *_partitionIDmap;       //  Map from global ID to partition ID
  uint32              *_readsPerPartition;    //  Number of reads per partition, probably not useful
  uint32              *_readsInThisPartition; //  A list of the (global ID) reads in this partition
};




class gkStoreStats {
public:
  gkStoreStats(char const *gkStoreName) {
    gkStore *gkp = new gkStore(gkStoreName);
    init(gkp);
    delete gkp;
  };

  gkStoreStats(gkStore *gkp) {
    init(gkp);
  };

  ~gkStoreStats() {
    delete [] lowestID;
    delete [] highestID;

    delete [] numActivePerLib;
    delete [] numDeletedPerLib;

    for (uint32 i=0; i<GKREAD_TYPE_NUM; i++)
      delete [] numByEncPerLib[i];

    delete [] readLengthPerLib;
    delete [] clearLengthPerLib;
  };

  void    init(gkStore *gkp);

  //  Global stats over the whole store

  uint32  numActiveFrag;
  uint32  numDeletedFrag;
  uint32  numByEnc[GKREAD_TYPE_NUM];
  uint64  readLength;
  uint64  clearLength;

  //  Per library stats

  uint32  *lowestID;
  uint32  *highestID;

  uint32  *numActivePerLib;
  uint32  *numDeletedPerLib;
  uint32 **numByEncPerLib;
  uint64  *readLengthPerLib;
  uint64  *clearLengthPerLib;
};


#endif