/* 7-Zip client for xadmaster.library
 * chris@unsatisfactorysoftware.co.uk
 */

#ifndef XADMASTER_7Z_C
#define XADMASTER_7Z_C

#include <proto/exec.h>

#ifdef __amigaos4__
struct ExecIFace *IExec;
struct Library *newlibbase;
struct Interface *INewlib;
#include "sys/types.h"
#else
#include <exec/types.h>
#endif

#include <string.h>
#include "7z.h"
#include "../7zCrc.h"
#include "../7zFile.h"
#include <proto/xadmaster.h>
#include <dos/dos.h>
#include "SDI_compiler.h"
#include <proto/exec.h>
#include <proto/utility.h>
#include "7zAlloc.h"

#ifndef XADMASTERFILE
#define sz_Client		FirstClient
#define NEXTCLIENT		0
#ifdef __amigaos4__
#define XADMASTERVERSION	13
#else
#define XADMASTERVERSION	12
#endif
UBYTE *version = VERSTAG; //"$VER: 7-Zip 1.4 (27.01.2008)";
#endif
#define SZ_VERSION	VERSION
#define SZ_REVISION	REVISION

//IExec = (struct ExecIFace *)(*(struct ExecBase **)4)->MainInterface; // TEMP
//DebugPrintF("reading %ld bytes\n",size);

#define kBufferSize (1 << 12)
Byte *g_buffer = 0;

SRes SzFileReadImp(void *object, void *buffer, size_t *size)
{
  CFileXadInStream *s = (CFileXadInStream *)object;
#ifdef __amigaos4__
	struct XadMasterIFace *IXadMaster = s->IXadMaster;
#else
	struct xadMasterBase *xadMasterBase = s->xadMasterBase;
#endif

	if(!g_buffer) g_buffer = AllocVec(kBufferSize, MEMF_PRIVATE);

  size_t processedSizeLoc =+ *size; // =+
	//s->posn = processedSizeLoc;

  if (*size > kBufferSize)
    *size = kBufferSize;

	if(xadHookAccess(XADAC_READ, *size, g_buffer, s->ai) != XADERR_OK)
		return SZ_ERROR_FAIL;

	memcpy(buffer, g_buffer, *size);

//	*buffer = g_buffer;
/*
  if (processedSize != 0)
    *processedSize = processedSizeLoc;
*/
  return SZ_OK;
}

SRes SzFileSeekImp(void *object, Int64 *pos, ESzSeek method)
{
  CFileXadInStream *s = (CFileXadInStream *)object;
#ifdef __amigaos4__
	struct XadMasterIFace *IXadMaster = s->IXadMaster;
#else
	struct xadMasterBase *xadMasterBase = s->xadMasterBase;
#endif
	int res=0;
//	ULONG posn = *pos >> 32;

	switch(method)
	{
		case SZ_SEEK_SET:
  			res = xadHookAccess(XADAC_INPUTSEEK, (long)*pos-(s->ai->xai_InPos), 0, s->ai);
		break;

		case SZ_SEEK_CUR:
  			res = xadHookAccess(XADAC_INPUTSEEK, (long)*pos, 0, s->ai);
		break;

		case SZ_SEEK_END:
  			res = xadHookAccess(XADAC_INPUTSEEK, (s->ai->xai_InSize) - (long)*pos - (s->ai->xai_InPos), 0, s->ai);
		break;
	}

  if (res == 0)
	{
		s->posn = *pos;
		*pos = s->ai->xai_InPos;
    	return SZ_OK;
	}
  return SZ_ERROR_FAIL;
}

#define WINDOWS_TICK 10000000
#define SEC_TO_AMIGA_EPOCH 11896070400LL

ULONG WindowsTickToAmigaSeconds(long long windowsTicks)
{
     return (ULONG)(windowsTicks / WINDOWS_TICK - SEC_TO_AMIGA_EPOCH);
}

ULONG ConvertFileTime(CNtfsFileTime *ft)
{
	UInt64 v64 = ft->Low | ((UInt64)ft->High << 32);
	return(WindowsTickToAmigaSeconds(v64));
}

long sztoxaderr(long res)
{
	long err=XADERR_OK;

	switch(res)
	{
// CAN ADD EXTRA ERRORS IN HERE! see lzma - types.h
		case SZ_OK:
			err=XADERR_OK;
		break;
		case SZ_ERROR_DATA:
			err=XADERR_ILLEGALDATA;
		break;
		case SZ_ERROR_MEM:
			err=XADERR_NOMEMORY;
		break;
		case SZ_ERROR_CRC:
			err=XADERR_CHECKSUM;
		break;
		case SZ_ERROR_UNSUPPORTED:
			err=XADERR_FILETYPE;
		break;
		case SZ_ERROR_FAIL:
			err=XADERR_DECRUNCH;
		break;
		case SZ_ERROR_ARCHIVE:
			err=XADERR_DATAFORMAT;
		break;
		default:
			err=XADERR_UNKNOWN;
		break;
	}
	return err;
}

#ifdef __amigaos4__
BOOL sz_RecogData(ULONG size, STRPTR data,
struct XadMasterIFace *IXadMaster)
#else
ASM(BOOL) sz_RecogData(REG(d0, ULONG size), REG(a0, STRPTR data),
REG(a6, struct xadMasterBase *xadMasterBase))
#endif
{
  if(data[0]=='7' & data[1]=='z' & data[2]==0xBC & data[3]==0xAF & data[4]==0x27 & data[5]==0x1C)
    return 1; /* known file */
  else
    return 0; /* unknown file */
}

#ifdef __amigaos4__
LONG sz_GetInfo(struct xadArchiveInfo *ai,
struct XadMasterIFace *IXadMaster)
#else
ASM(LONG) sz_GetInfo(REG(a0, struct xadArchiveInfo *ai),
REG(a6, struct xadMasterBase *xadMasterBase))
#endif
{
#ifdef __amigaos4__
    IExec = (struct ExecIFace *)(*(struct ExecBase **)4)->MainInterface;

    newlibbase = OpenLibrary("newlib.library", 52);
    if(newlibbase)
        INewlib = GetInterface(newlibbase, "main", 1, NULL);
#else
 SysBase = *(struct ExecBase **)4;
#endif

	ai->xai_PrivateClient = xadAllocVec(sizeof(struct xad7zprivate),MEMF_PRIVATE | MEMF_CLEAR);
	struct xad7zprivate *xad7z = ai->xai_PrivateClient;
  struct xadFileInfo *fi;
	long err=XADERR_OK;
	long res=SZ_OK;
	size_t namelen;
	UBYTE *namebuf; // was UWORD
  CFileXadInStream *archiveStream = &xad7z->archiveStream;
  CSzArEx *db = &xad7z->db;       /* 7z archive database structure */
  ISzAlloc allocImp;           /* memory functions for main pool */
  ISzAlloc allocTempImp;       /* memory functions for temporary pool */
	CLookToRead *lookStream = &xad7z->lookStream;

  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;
  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;

  archiveStream->ai = ai;
#ifdef __amigaos4__
  archiveStream->IXadMaster = IXadMaster;
#else
  archiveStream->xadMasterBase = xadMasterBase;
#endif
  archiveStream->InStream.Read = SzFileReadImp;
  archiveStream->InStream.Seek = SzFileSeekImp;

	if(namebuf = AllocVec(1024, MEMF_PRIVATE))
	{

  LookToRead_CreateVTable(lookStream, False);
  lookStream->realStream = (ISeekInStream *)&archiveStream->InStream;
  LookToRead_Init(lookStream);

	xad7z->blockIndex = 0xfffffff;
	xad7z->outBuffer = 0;
	xad7z->outBufferSize = 0;

  CrcGenerateTable();
  SzArEx_Init(db);
  res = SzArEx_Open(db, &lookStream->s, &allocImp, &allocTempImp);

	if(res == SZ_OK)
    {
      UInt32 i;

      for (i = 0; i < db->NumFiles; i++)
      {
    fi = (struct xadFileInfo *) xadAllocObjectA(XADOBJ_FILEINFO, NULL);
    if (!fi) return(XADERR_NOMEMORY);
//        CSzFileItem *f = db->db.Files + i;
		fi->xfi_DataPos = 0; //ai->xai_InPos; // i
		fi->xfi_Size = SzArEx_GetFileSize(db, i);

	namelen = SzArEx_GetFileNameUtf16(db, i, namebuf); // &
    if (!(fi->xfi_FileName = xadConvertName(CHARSET_HOST,
							XAD_CHARACTERSET, CHARSET_UNICODE_UCS2_BIGENDIAN,
							XAD_STRINGSIZE, namelen,
							XAD_CSTRING, namebuf, // no *
							TAG_DONE))) return(XADERR_NOMEMORY);

    xadConvertDates(XAD_DATEAMIGA, ConvertFileTime(&db->MTime),
					XAD_GETDATEXADDATE, &fi->xfi_Date,
					TAG_DONE);

      fi->xfi_CrunchSize  = 0; //(long) (db->Database.PackSizes[i] << 32); //fi->xfi_Size;

	fi->xfi_Flags = 0;
		if(SzArEx_IsDir(db, i))
		{
			fi->xfi_Flags |= XADFIF_DIRECTORY;
		}

		 if ((err = xadAddFileEntryA(fi, ai, NULL))) return(XADERR_NOMEMORY);
      }
    }
	FreeVec(namebuf);
	}

	return(sztoxaderr(res));
}

#ifdef __amigaos4__
LONG sz_UnArchive(struct xadArchiveInfo *ai,
struct XadMasterIFace *IXadMaster)
#else
ASM(LONG) sz_UnArchive(REG(a0, struct xadArchiveInfo *ai),
REG(a6, struct xadMasterBase *xadMasterBase))
#endif
{
	struct xad7zprivate *xad7z = ai->xai_PrivateClient;
  struct xadFileInfo *fi = ai->xai_CurFile;
	long err=XADERR_OK;
  CFileXadInStream *archiveStream = &xad7z->archiveStream;
  CSzArEx *db = &xad7z->db;       /* 7z archive database structure */
  ISzAlloc allocImp;           /* memory functions for main pool */
  ISzAlloc allocTempImp;       /* memory functions for temporary pool */
	CLookToRead *lookStream = &xad7z->lookStream;

	UInt32 *blockIndex = &xad7z->blockIndex;
      Byte **outBuffer = &xad7z->outBuffer; /* it must be 0 before first call for each new archive. */
      size_t *outBufferSize = &xad7z->outBufferSize;  /* it can have any value before first call (if outBuffer = 0) */
        size_t offset = 0;
        size_t outSizeProcessed = 0;
	int res=0;

  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;
  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;

#ifdef __amigaos4__
	if(!newlibbase)
	{
    	IExec = (struct ExecIFace *)(*(struct ExecBase **)4)->MainInterface;
	    newlibbase = OpenLibrary("newlib.library", 52);
    	if(newlibbase)
        	INewlib = GetInterface(newlibbase, "main", 1, NULL);
	}
#else
 SysBase = *(struct ExecBase **)4;
#endif

  res = SzArEx_Extract(
    db,
    &lookStream->s, 
    (UInt32)fi->xfi_EntryNumber - 1,         /* index of file */
    blockIndex,       /* index of solid block */
    outBuffer,         /* pointer to pointer to output buffer (allocated with allocMain) */
    outBufferSize,    /* buffer size for output buffer */
    &offset,           /* offset of stream for required file in *outBuffer */
    &outSizeProcessed, /* size of file in *outBuffer */
    &allocImp,
    &allocTempImp);

	if(res==SZ_OK)
	{
		/* test loopy stuff */
		UBYTE *tempmem = NULL;
		long bytes = 0,writebytes=0;

		tempmem = AllocVec(1024,MEMF_CLEAR);
		if(!tempmem) return XADERR_NOMEMORY;

		for(bytes = outSizeProcessed;bytes>0;bytes-=1024)
		{
			if(bytes>1024)
			{
				writebytes = 1024;
			}
			else
			{
				writebytes = bytes;
			}

			CopyMem((*outBuffer)+offset+(outSizeProcessed-bytes),tempmem,writebytes);
			err = xadHookAccess(XADAC_WRITE,writebytes,tempmem,ai);
			if(err != XADERR_OK) break;
//			xadHookAccess(XADAC_WRITE,outSizeProcessed,(*outBuffer)+offset, ai);

		}
		FreeVec(tempmem);
	}

	if(err==XADERR_OK) err=sztoxaderr(res);

	return err;
}

#ifdef __amigaos4__
void sz_Free(struct xadArchiveInfo *ai,
struct XadMasterIFace *IXadMaster)
#else
ASM(void) sz_Free(REG(a0, struct xadArchiveInfo *ai),
REG(a6, struct xadMasterBase *xadMasterBase))
#endif
{
  /* This function needs to free all the stuff allocated in info or
  unarchive function. It may be called multiple times, so clear freed
  entries!
  */
#ifdef __amigaos4__
    IExec = (struct ExecIFace *)(*(struct ExecBase **)4)->MainInterface;

    newlibbase = OpenLibrary("newlib.library", 52);
    if(newlibbase)
        INewlib = GetInterface(newlibbase, "main", 1, NULL);
#else
 SysBase = *(struct ExecBase **)4;
#endif

  ISzAlloc allocImp;           /* memory functions for main pool */
  ISzAlloc allocTempImp;       /* memory functions for temporary pool */
  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;
  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;

	struct xad7zprivate *xad7z = ai->xai_PrivateClient;

  CSzArEx *db = &xad7z->db;
	UInt32 *blockIndex = &xad7z->blockIndex;
	Byte **outBuffer = &xad7z->outBuffer;
	size_t *outBufferSize = &xad7z->outBufferSize;

	allocImp.Free(NULL,*outBuffer); // was commented out
	SzArEx_Free(db, &allocImp);

	*outBuffer=0;
	*blockIndex = 0xfffffff;
	*outBufferSize = 0;

	if(g_buffer)
	{
		FreeVec(g_buffer);
		g_buffer = NULL;
	}

//	CrcFreeTable();

	xadFreeObjectA(ai->xai_PrivateClient,NULL);
	ai->xai_PrivateClient = NULL;

#ifdef __amigaos4__
    DropInterface(INewlib);
    CloseLibrary(newlibbase);
	INewlib = NULL;
	newlibbase = NULL;
#endif

}

/* You need to complete following structure! */
const struct xadClient sz_Client = {
NEXTCLIENT, XADCLIENT_VERSION, XADMASTERVERSION, SZ_VERSION, SZ_REVISION,
6, XADCF_FILEARCHIVER|XADCF_FREEFILEINFO|XADCF_FREEXADSTRINGS,
0 /* Type identifier. Normally should be zero */, "7-Zip",
(BOOL (*)()) sz_RecogData, (LONG (*)()) sz_GetInfo,
(LONG (*)()) sz_UnArchive, (void (*)()) sz_Free };

#ifndef __amigaos4__
void main(void)
{
}
#endif
#endif /* XADMASTER_7Z_C */
