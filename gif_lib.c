//
// GIFLIB-turbo
//
// Copyright (c) 2021 BitBank Software, Inc.
// written by Larry Bank
// Project started 2/11/2021
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
// Use POSIX I/O for compatibility with Linux/MacOS/Windows
#include <unistd.h>
#include <sys/fcntl.h>

#include "gif_lib.h"

static const uint8_t cGIFPass[8] = {8,0,8,4,4,2,2,1}; // GIF interlaced y delta
int EncodeLZW(SavedImage *pImage, uint32_t *pSymbols, uint8_t *pOutput, uint8_t ucCodeStart);
//
// Macro to write a variable length code to the output buffer
//
#define GIFOUTPUT(code, nbits) \
{ \
unsigned char *d; \
   u64Out |= (BIGUINT)code << bitoff; \
   bitoff += nbits; \
   if (bitoff > (REGISTER_WIDTH - MAX_CODE_LEN - 1)) { /* can't let it reach 64 bits exactly, undefined right shift */ \
      d = pOutput + byteoff; \
      byteoff += (bitoff >> 3); \
      *(BIGUINT *)d = u64Out; /* store multiple bytes of codes */ \
      u64Out >>= (bitoff & 0xf8); \
      bitoff &= 7; \
   } \
}

const char *GifErrorString(int ErrorCode)
{
    const char *Err;

    switch (ErrorCode) {
      case E_GIF_ERR_OPEN_FAILED:
        Err = "Failed to open given file";
        break;
      case E_GIF_ERR_WRITE_FAILED:
        Err = "Failed to write to given file";
        break;
      case E_GIF_ERR_HAS_SCRN_DSCR:
        Err = "Screen descriptor has already been set";
        break;
      case E_GIF_ERR_HAS_IMAG_DSCR:
        Err = "Image descriptor is still active";
        break;
      case E_GIF_ERR_NO_COLOR_MAP:
        Err = "Neither global nor local color map";
        break;
      case E_GIF_ERR_DATA_TOO_BIG:
        Err = "Number of pixels bigger than width * height";
        break;
      case E_GIF_ERR_NOT_ENOUGH_MEM:
        Err = "Failed to allocate required memory";
        break;
      case E_GIF_ERR_DISK_IS_FULL:
        Err = "Write failed (disk full?)";
        break;
      case E_GIF_ERR_CLOSE_FAILED:
        Err = "Failed to close given file";
        break;
      case E_GIF_ERR_NOT_WRITEABLE:
        Err = "Given file was not opened for write";
        break;
      case D_GIF_ERR_OPEN_FAILED:
        Err = "Failed to open given file";
        break;
      case D_GIF_ERR_READ_FAILED:
        Err = "Failed to read from given file";
        break;
      case D_GIF_ERR_NOT_GIF_FILE:
        Err = "Data is not in GIF format";
        break;
      case D_GIF_ERR_NO_SCRN_DSCR:
        Err = "No screen descriptor detected";
        break;
      case D_GIF_ERR_NO_IMAG_DSCR:
        Err = "No Image Descriptor detected";
        break;
      case D_GIF_ERR_NO_COLOR_MAP:
        Err = "Neither global nor local color map";
        break;
      case D_GIF_ERR_WRONG_RECORD:
        Err = "Wrong record type detected";
        break;
      case D_GIF_ERR_DATA_TOO_BIG:
        Err = "Number of pixels bigger than width * height";
        break;
      case D_GIF_ERR_NOT_ENOUGH_MEM:
        Err = "Failed to allocate required memory";
        break;
      case D_GIF_ERR_CLOSE_FAILED:
        Err = "Failed to close given file";
        break;
      case D_GIF_ERR_NOT_READABLE:
        Err = "Given file was not opened for read";
        break;
      case D_GIF_ERR_IMAGE_DEFECT:
        Err = "Image is defective, decoding aborted";
        break;
      case D_GIF_ERR_EOF_TOO_SOON:
        Err = "Image EOF detected before image complete";
        break;
      default:
        Err = NULL;
        break;
    }
    return Err;
}

void PrintGifError(int ErrorCode) {
    const char *Err = GifErrorString(ErrorCode);

    if (Err != NULL)
        fprintf(stderr, "GIF-LIB error: %s.\n", Err);
    else
        fprintf(stderr, "GIF-LIB undefined error %d.\n", ErrorCode);
} /* PrintGifError() */

//
// EGifSpew
//
// Create a multi-frame GIF output file
//
int EGifSpew(GifFileType * gif)
{
    int rc = GIF_OK;
    int i, iChunk, iFrame, iSize;
    uint8_t c, *p, *pLZW = NULL; // buffer holding the compressed data for each frame
    uint8_t *pChunked = NULL; // temp area for preparing chunked data
    int iLen;
    GIFPRIVATE *pPrivate = (GIFPRIVATE *)gif->Private;
    
    pLZW = (uint8_t *)malloc((gif->SWidth * gif->SHeight * 3)/2); // allow for worst case
    if (pLZW == NULL) {
        return E_GIF_ERR_NOT_ENOUGH_MEM;
    }
    pChunked = (uint8_t *)malloc(gif->SWidth * gif->SHeight * 2);
    if (pChunked == NULL) {
        free(pLZW);
        return E_GIF_ERR_NOT_ENOUGH_MEM;
    }
    // Prepare GIF header
    pChunked[0] = 'G';
    pChunked[1] = 'I';
    pChunked[2] = 'F';
    pChunked[3] = '8';
    pChunked[4] = '9';
    pChunked[5] = 'a';
    *(uint16_t *)&pChunked[6] = gif->SWidth;
    *(uint16_t *)&pChunked[8] = gif->SHeight;
    iLen = 10;
    // Create colormap flag bits
    c = 0x7 | ((gif->SColorResolution - 1) << 4); // no colortable?
    if (gif->SColorMap->ColorCount) {
        c = 0x80;
        c |= ((gif->SColorResolution - 1) << 4); // bits allocated to each primary color
        c |= gif->SColorMap->BitsPerPixel - 1; // actual size of the color table
    }
    pChunked[iLen++] = c;
    pChunked[iLen++] = gif->SBackGroundColor;
    pChunked[iLen++] = 0; // future expansion
    // global palette entries
    i = gif->SColorMap->ColorCount; // palette size
    memcpy(&pChunked[iLen], gif->SColorMap->Colors, i * 3);
    iLen += i * 3; // RGB palette entries
#ifdef FUTURE
    // Netscape 2.0 looping block must be placed right after global color table
    if (gif->ImageCount > 1) {
        pChunked[iLen++] = '!';
        pChunked[iLen++] = 0xff; // app extension
        pChunked[iLen++] = 11; // length
        pChunked[iLen++] = 'N';
        pChunked[iLen++] = 'E';
        pChunked[iLen++] = 'T';
        pChunked[iLen++] = 'S';
        pChunked[iLen++] = 'C';
        pChunked[iLen++] = 'A';
        pChunked[iLen++] = 'P';
        pChunked[iLen++] = 'E';
        pChunked[iLen++] = '2';
        pChunked[iLen++] = '.';
        pChunked[iLen++] = '0';
        pChunked[iLen++] = 3; // length of sub-block
        pChunked[iLen++] = 1;
        pChunked[iLen++] = 0; // 16-bit repeat count
        pChunked[iLen++] = 0;
        pChunked[iLen++] = 0; // length terminator
    }
#endif // FUTURE
    // Do the rest of the frames as deltas from the first
    for (iFrame=0; iFrame < gif->ImageCount; iFrame++) // for each frame after initial
    {
        SavedImage *pSI = &gif->SavedImages[iFrame];
        for (int iExt=0; iExt < pSI->ExtensionBlockCount; iExt++) { // add extension(s)
            ExtensionBlock *pEB = &pSI->ExtensionBlocks[iExt];
            pChunked[iLen++] = '!';
            pChunked[iLen++] = pEB->Function; // e.g. 0xf9;
            pChunked[iLen++] = pEB->ByteCount;
            memcpy(&pChunked[iLen], pEB->Bytes, pEB->ByteCount);
            iLen += pEB->ByteCount;
            // write any continuation blocks
            while (iExt < pSI->ExtensionBlockCount && gif->SavedImages[iFrame].ExtensionBlocks[iExt+1].Function == 0 && gif->SavedImages[iFrame].ExtensionBlocks[iExt+1].ByteCount > 0) {
                iExt++;
                ExtensionBlock *pEB = &pSI->ExtensionBlocks[iExt];
                pChunked[iLen++] = pEB->ByteCount;
                memcpy(&pChunked[iLen], pEB->Bytes, pEB->ByteCount);
                iLen += pEB->ByteCount;
            }
            pChunked[iLen++] = 0; // terminating 0
        } // for each extension block
        pChunked[iLen++] = ',';
        pChunked[iLen++] = (uint8_t)gif->SavedImages[iFrame].ImageDesc.Left; /* Image position - 4 bytes*/
        pChunked[iLen++] = (uint8_t)(gif->SavedImages[iFrame].ImageDesc.Left >> 8);
        pChunked[iLen++] = (uint8_t)gif->SavedImages[iFrame].ImageDesc.Top;
        pChunked[iLen++] = (uint8_t)(gif->SavedImages[iFrame].ImageDesc.Top >> 8);
        pChunked[iLen++] = (uint8_t)gif->SavedImages[iFrame].ImageDesc.Width;  /* Image size */
        pChunked[iLen++] = (uint8_t)(gif->SavedImages[iFrame].ImageDesc.Width >> 8);
        pChunked[iLen++] = (uint8_t)gif->SavedImages[iFrame].ImageDesc.Height;
        pChunked[iLen++] = (uint8_t)(gif->SavedImages[iFrame].ImageDesc.Height >> 8);
        if (gif->SavedImages[iFrame].ImageDesc.ColorMap) { // local color table?
            c = 0x80 | (gif->SavedImages[iFrame].ImageDesc.ColorMap->BitsPerPixel - 1);
            pChunked[iLen++] = c;
            i = gif->SavedImages[iFrame].ImageDesc.ColorMap->ColorCount; // palette size
            memcpy(&pChunked[iLen], gif->SavedImages[iFrame].ImageDesc.ColorMap->Colors, i * 3);
            iLen += i * 3; // RGB palette entries
        } else {
            pChunked[iLen++] = 0; // no local color table
        }
        pChunked[iLen++] = gif->SColorResolution;
        iSize = EncodeLZW(&gif->SavedImages[iFrame], pPrivate->pSymbols, pLZW, gif->SColorResolution);
//        if (iSize <= 0) { // something went wrong
//            rc = GIF_ENCODE_ERROR;
//            goto gif_create_exit;
//        }
        p = pLZW;
        // Now starts the chunked data for this frame
        i = 0; // offset to compressed data
        while (i < iSize) { // chunk it
            iChunk = 255; // max 255 bytes per chunk
            if (iChunk > (iSize - i)) iChunk = (iSize - i);
            pChunked[iLen++] = (uint8_t)iChunk;
            memcpy(&pChunked[iLen], &p[i], iChunk);
            iLen += iChunk;
            i += iChunk;
        }
        // all compressed data from the frame it done
        pChunked[iLen++] = 0; // no more data
        write(pPrivate->iHandle, pChunked, iLen);
        iLen = 0;
    } // for each frame
gif_create_exit:
    write(pPrivate->iHandle, ";", 1); // finish the file here
    close(pPrivate->iHandle);
    if (pLZW) free(pLZW);
    if (pChunked) free(pChunked);
    EGifCloseFile(gif, &rc);
    return rc;
} /* EGifSpew() */
//
// GIFInterlace
//
void GIFInterlace(uint8_t *pSrc, int iWidth, int iHeight)
{
    int iGifPass = 0;
    int i, y;
    uint8_t *d, *s;
    uint8_t *pTemp = malloc(iWidth * iHeight);
    
    y = 0;
    for (i = 0; i < iHeight; i++)
    {
        s = &pSrc[y * iWidth];
        d = &pTemp[i * iWidth];
        memcpy(d, s, iWidth);
        y += cGIFPass[iGifPass * 2];
        if (y >= iHeight)
        {
            iGifPass++;
            y = cGIFPass[iGifPass * 2 + 1];
        }
    }
    memcpy(pSrc, pTemp, iWidth * iHeight); // copy it back over source image
    free(pTemp);

} /* GIFInterlace() */
//
// GifBitSize
//
int GifBitSize(int n)
{
    register int i;

    for (i = 1; i <= 8; i++)
        if ((1 << i) >= n)
            break;
    return (i);
} /* GifBitSize() */
//
// GifFreeMapObject
//
void GifFreeMapObject(ColorMapObject *Object)
{
    if (Object != NULL) {
        (void)free(Object->Colors);
        (void)free(Object);
    }
} /* GifFreeMapObject() */
//
// GifMakeMapObject
//
ColorMapObject *GifMakeMapObject(int ColorCount, const GifColorType *ColorMap)
{
    ColorMapObject *Object;

    if (ColorCount != (1 << GifBitSize(ColorCount))) {
        return ((ColorMapObject *) NULL);
    }

    Object = (ColorMapObject *)malloc(sizeof(ColorMapObject));
    if (Object == (ColorMapObject *) NULL) {
        return ((ColorMapObject *) NULL);
    }

    Object->Colors = (GifColorType *)calloc(ColorCount, sizeof(GifColorType));
    if (Object->Colors == (GifColorType *) NULL) {
        free(Object);
        return ((ColorMapObject *) NULL);
    }

    Object->ColorCount = ColorCount;
    Object->BitsPerPixel = GifBitSize(ColorCount);
    Object->SortFlag = false;

    if (ColorMap != NULL) {
        memcpy((char *)Object->Colors,
               (char *)ColorMap, ColorCount * sizeof(GifColorType));
    }

    return (Object);
} /* GifMakeMapObject() */
//
// GifFreeExtensions
//
void GifFreeExtensions(int *ExtensionBlockCount, ExtensionBlock **ExtensionBlocks)
{
    ExtensionBlock *ep;

    if (*ExtensionBlocks == NULL)
        return;

    for (ep = *ExtensionBlocks;
         ep < (*ExtensionBlocks + *ExtensionBlockCount);
         ep++)
        (void)free((char *)ep->Bytes);
    (void)free((char *)*ExtensionBlocks);
    *ExtensionBlocks = NULL;
    *ExtensionBlockCount = 0;
} /* GifFreeExtensions() */
//
// FreeLastSavedImage
//
void FreeLastSavedImage(GifFileType *GifFile)
{
    SavedImage *sp;

    if ((GifFile == NULL) || (GifFile->SavedImages == NULL))
        return;

    /* Remove one SavedImage from the GifFile */
    GifFile->ImageCount--;
    sp = &GifFile->SavedImages[GifFile->ImageCount];

    /* Deallocate its Colormap */
    if (sp->ImageDesc.ColorMap != NULL) {
        GifFreeMapObject(sp->ImageDesc.ColorMap);
        sp->ImageDesc.ColorMap = NULL;
    }

    /* Deallocate the image data */
    if (sp->RasterBits != NULL)
        free((char *)sp->RasterBits);

    /* Deallocate any extensions */
    GifFreeExtensions(&sp->ExtensionBlockCount, &sp->ExtensionBlocks);

    /*** FIXME: We could realloc the GifFile->SavedImages structure but is
     * there a point to it? Saves some memory but we'd have to do it every
     * time.  If this is used in GifFreeSavedImages then it would be inefficient
     * (The whole array is going to be deallocated.)  If we just use it when
     * we want to free the last Image it's convenient to do it here.
     */
} /* FreeLastSavedImage() */
/*
 * Append an image block to the SavedImages array
 */
SavedImage *GifMakeSavedImage(GifFileType *GifFile, const SavedImage *CopyFrom)
{
    if (GifFile->SavedImages == NULL)
        GifFile->SavedImages = (SavedImage *)malloc(sizeof(SavedImage));
    else {
        SavedImage* newSavedImages = (SavedImage *)realloc(GifFile->SavedImages,
                               (GifFile->ImageCount + 1) * sizeof(SavedImage));
        if( newSavedImages == NULL)
            return ((SavedImage *)NULL);
        GifFile->SavedImages = newSavedImages;
    }
    if (GifFile->SavedImages == NULL)
        return ((SavedImage *)NULL);
    else {
        SavedImage *sp = &GifFile->SavedImages[GifFile->ImageCount++];

        if (CopyFrom != NULL) {
            memcpy((char *)sp, CopyFrom, sizeof(SavedImage));

            /*
             * Make our own allocated copies of the heap fields in the
             * copied record.  This guards against potential aliasing
             * problems.
             */

            /* first, the local color map */
            if (CopyFrom->ImageDesc.ColorMap != NULL) {
                sp->ImageDesc.ColorMap = GifMakeMapObject(
                                         CopyFrom->ImageDesc.ColorMap->ColorCount,
                                         CopyFrom->ImageDesc.ColorMap->Colors);
                if (sp->ImageDesc.ColorMap == NULL) {
                    FreeLastSavedImage(GifFile);
                    return (SavedImage *)(NULL);
                }
            }

            /* next, the raster */
            sp->RasterBits = (unsigned char *)malloc(                             (CopyFrom->ImageDesc.Height *                                           CopyFrom->ImageDesc.Width) *                                          sizeof(GifPixelType));
            if (sp->RasterBits == NULL) {
                FreeLastSavedImage(GifFile);
                return (SavedImage *)(NULL);
            }
            memcpy(sp->RasterBits, CopyFrom->RasterBits,
                   sizeof(GifPixelType) * CopyFrom->ImageDesc.Height *
                   CopyFrom->ImageDesc.Width);

            /* finally, the extension blocks */
            if (CopyFrom->ExtensionBlocks != NULL) {
                sp->ExtensionBlocks = (ExtensionBlock *)calloc(1,                                MAX_EXTENSIONS *                                  sizeof(ExtensionBlock));
                if (sp->ExtensionBlocks == NULL) {
                    FreeLastSavedImage(GifFile);
                    return (SavedImage *)(NULL);
                }
                memcpy(sp->ExtensionBlocks, CopyFrom->ExtensionBlocks,
                       sizeof(ExtensionBlock) * CopyFrom->ExtensionBlockCount);
            }
        }
        else {
            memset((char *)sp, '\0', sizeof(SavedImage));
        }

        return (sp);
    }
} /* GifMakeSavedImage() */

//
// Compress a GIF image with LZW
//
int EncodeLZW(SavedImage *pImage, uint32_t *pSymbols, uint8_t *pOutput, uint8_t ucCodeStart)
{
int i, iMAXMAX;
int init_bits, nbits, bitoff, byteoff;
unsigned char *p;
BIGUINT u64Out;
short *codetab, disp, code, maxcode, cc, free_ent, eoi;
BIGINT lastentry;
int32_t hashcode, cvar, *hashtab;
int iRemainingPixels = pImage->ImageDesc.Height * pImage->ImageDesc.Width;
    
    u64Out = 0;
    bitoff = byteoff = 0;
    init_bits = ucCodeStart + 1;
    p = (unsigned char *)pOutput;
    iMAXMAX = MAXMAXCODE;
    nbits = init_bits;
    hashtab = (int32_t *)pSymbols;
    codetab = (short *)&pSymbols[MAX_HASH+8];
    cc = 1 << (nbits - 1);
    eoi = cc + 1;
    lastentry = 0; /* To suppress compiler warning */
    free_ent = eoi + 1;
    maxcode = (1 << nbits) - 1;

  /* Clear the hash table */
  for (i=0; i<MAX_HASH; i++)
     hashtab[i] = -1;
  GIFOUTPUT(cc, nbits); /* Start by encoding a cc */
  p = pImage->RasterBits;
  lastentry = *p++; /* Get first pixel to start */
    iRemainingPixels--;
  while (iRemainingPixels)
  {
      cvar = *p++; /* Grab a character to compress */
      iRemainingPixels--;
      hashcode = (cvar << 12) + (int32_t)lastentry;
      code = (short)((cvar << 4) ^ lastentry);
      if (hashcode == hashtab[code])
      {
          lastentry = codetab[code];
          continue;
      }
      else
      {
         if (hashtab[code] == -1)
             goto gif_nomatch;
         disp = MAX_HASH - code;
         if (code == 0)
             disp = 1;
gif_probe:
          code -= disp;
          if (code < 0)
             code += MAX_HASH;
          if (hashtab[code] == hashcode)
          {
              lastentry = codetab[code];
              continue;
          }
          if (hashtab[code] > 0)
              goto gif_probe;
gif_nomatch:
          GIFOUTPUT(lastentry, nbits); /* encode this one */
          lastentry = (short)cvar;
          /* Check for code size increase/clear flag */
          if (free_ent > maxcode)
          {
              nbits++;
              maxcode = (1 << nbits) - 1;
          }
          if (free_ent < iMAXMAX)
          {
             codetab[code] = free_ent++;
             hashtab[code] = hashcode;
          }
          else /* reset all tables */
          {
             free_ent = cc + 2;
             if (nbits == 13)
                 nbits--; /* Bit count is wrong */
             GIFOUTPUT(cc, nbits); /* encode this one */
             memset(hashtab, 0xff, MAX_HASH * sizeof(int32_t));
             nbits = init_bits;
             maxcode = (1 << nbits) - 1;
          }
       }
     } /* for pixel */
    /* Output the final code */
    GIFOUTPUT(lastentry, nbits); /* encode this one */
    GIFOUTPUT(eoi, nbits); /* End of image */
    p = pOutput + byteoff;
    *(BIGUINT *)p = u64Out; // store final code(s)
    byteoff += (bitoff >> 3);
    if (bitoff & 7)
        byteoff++; // partial byte
    return byteoff; // data size
} /* EncodeLZW() */
//
// EGifOpenFileName
//
GifFileType *EGifOpenFileName(const char *fname, const bool TestExistence, int *pError)
{
    int FileHandle;
    GifFileType *gif;

    if (TestExistence)
        FileHandle = open(fname, O_WRONLY | O_CREAT | O_EXCL,
              S_IREAD | S_IWRITE);
    else
        FileHandle = open(fname, O_WRONLY | O_CREAT | O_TRUNC,
              S_IREAD | S_IWRITE);

    if (FileHandle == -1) {
        if (pError != NULL)
        *pError = E_GIF_ERR_OPEN_FAILED;
        return NULL;
    }
    gif = EGifOpenFileHandle(FileHandle, pError);
    if (gif == NULL)
        (void)close(FileHandle);
    return gif;
} /* EGifOpenFileName() */
//
// EGifOpenFileHandle
//
GifFileType *EGifOpenFileHandle(const int FileHandle, int *pError)
{
    GifFileType *gif;
    GIFPRIVATE *pPrivate;

    gif = (GifFileType *) calloc(1, sizeof(GifFileType));
    if (gif == NULL) {
        return NULL;
    }

    pPrivate = (GIFPRIVATE *)calloc(1, sizeof(GIFPRIVATE));
    if (pPrivate == NULL) {
        free(gif);
        if (pError != NULL)
        *pError = E_GIF_ERR_NOT_ENOUGH_MEM;
        return NULL;
    }
    pPrivate->pSymbols = malloc(3 * 4096 * sizeof(uint32_t));
    if (pPrivate->pSymbols == NULL) {
        free(gif);
        free(pPrivate);
        if (pError != NULL)
        *pError = E_GIF_ERR_NOT_ENOUGH_MEM;
        return NULL;
    }

#ifdef _WIN32
    _setmode(FileHandle, O_BINARY);    /* Make sure it is in binary mode. */
#endif /* _WIN32 */

    gif->Private = (void *)pPrivate;
    pPrivate->iHandle = FileHandle;
    gif->UserData = (void *)NULL;    /* No user write handle (MRB) */
    gif->Error = 0;

    return gif;
} /* EGifOpenFileHandle() */
//
// EGifOpen
//
GifFileType *EGifOpen(void *userPtr, OutputFunc writeFunc, int *Error)
{
    return NULL;
    // DEBUG
} /* EGifOpen() */

const char *EGifGetGifVersion(GifFileType *GifFile)
{
    return GIF89_STAMP; // we only create GIF89 files
} /* EGifGetGifVersion() */

int EGifCloseFile(GifFileType *gif, int *ErrorCode)
{
    int err = GIF_OK;
    if (gif->Private) {
        GIFPRIVATE *pPrivate = (GIFPRIVATE *)gif->Private;
        if (pPrivate->iHandle) {
            close(pPrivate->iHandle);
        }
        if (pPrivate->pSymbols)
            free(pPrivate->pSymbols);
        free(pPrivate);
        gif->Private = NULL;
    }
    if (gif->Image.ColorMap) {
        GifFreeMapObject(gif->Image.ColorMap);
        gif->Image.ColorMap = NULL;
    }
    if (gif->SColorMap) {
        GifFreeMapObject(gif->SColorMap);
        gif->SColorMap = NULL;
    }
    free(gif);
    return err;
} /* EGifCloseFile() */

//
// LZWCopyBytes
//
// Output the bytes for a single code (checks for buffer len)
//
static int LZWCopyBytes(unsigned char *buf, int iOffset, int iUncompressedLen, uint32_t *pSymbols)
{
int iLen;
uint8_t *s, *d;
int iTempLen;
uint32_t u32Offset;
    
    iLen = pSymbols[SYM_LENGTHS];
    u32Offset = pSymbols[SYM_EXTRAS];
    // Make sure data does not write past end of buffer (which does occur frequently)
    if (iLen > (iUncompressedLen - iOffset))
       iLen = iUncompressedLen - iOffset;
    s = &buf[pSymbols[SYM_OFFSETS]];
    d = &buf[iOffset];
    iTempLen = iLen;
    while (iTempLen > 0) // most frequent are 1-8 bytes in length, copy 8 bytes in these cases too
    {
#if REGISTER_WIDTH == 64
        BIGUINT tmp = *(BIGUINT *) s;
        s += sizeof(BIGUINT);
        iTempLen -= sizeof(BIGUINT);
        *(BIGUINT *)d = tmp;
        d += sizeof(BIGUINT);
#else
// 32-bit CPUs might enforce unaligned address exceptions
// Many Linux ARM systems do this
        *d++ = *s++;
        iTempLen--;
#endif
    }
    d += iTempLen; // in case we overshot
    if (u32Offset != 0xffffffff) // was a newly used code
    {
        s = &buf[u32Offset];
        iLen++;
        // since the code with extension byte has now been written to the output, fix the code
        pSymbols[SYM_OFFSETS] = iOffset;
        pSymbols[SYM_EXTRAS] = 0xffffffff;
        *d = *s;
        pSymbols[SYM_LENGTHS] = iLen;
    }
    return iLen;
} /* LZWCopyBytes() */
//
// DecodeLZW
//
// Theory of operation:
//
// The 'traditional' LZW decoder maintains a dictionary with a linked list of codes.
// These codes build into longer chains as more data is decoded. To output the pixels,
// the linked list is traversed backwards from the last node to the first, then these
// pixels are copied in reverse order to the output bitmap.
//
// My decoder takes a different approach. The output image becomes the dictionary and
// the tables keep track of where in the output image the 'run' begins and its length.
//
// I also work with the compressed data differently. Most decoders wind their way through
// the chunked data by constantly checking if the current chunk has run out of data. I
// take a different approach since modern machines have plenty of memory - I 'de-chunk'
// the data first so that the inner loop can just decode as fast as possible. I also keep
// a set of codes in a 64-bit local variable to minimize memory reads.
//
// These 2 changes result in a much faster decoder. For poorly compressed images, the
// speed gain is about 2.5x compared to giflib. For well compressed images (long runs)
// the speed can be as much as 30x faster. This is because my code doesn't have to walk
// backwards through the linked list of codes when outputting pixels. It also doesn't
// have to copy pixels in reverse order, then unwind them.
//
int DecodeLZW(GifFileType *gif, SavedImage *pPage, uint8_t ucCodeStart, uint8_t *pLZW, int iLZWSize)
{
int i, bitnum;
int iUncompressedLen;
uint32_t code, oldcode, codesize, nextcode, nextlim;
uint32_t cc, eoi;
uint32_t sMask;
unsigned char c, *p, *buf, codestart;
BIGUINT ulBits;
int iLen, iColors;
int iErr = GIF_OK;
int iOffset;
GIFPRIVATE *pPrivate = gif->Private;
uint32_t *pSymbols;

    p = pLZW;
    ulBits = *(BIGUINT *)p;
    bitnum = 0;
    codestart = ucCodeStart;
    iColors = 1 << codestart;
    sMask = -1 << (codestart+1);
    sMask = 0xffffffff - sMask;
    cc = (sMask >> 1) + 1; /* Clear code */
    eoi = cc + 1;
    pSymbols = pPrivate->pSymbols;
    iUncompressedLen = (pPage->ImageDesc.Width * pPage->ImageDesc.Height);
    buf = pPage->RasterBits;
    iOffset = 0; // output data offset

init_codetable:
   for (i = 0; i<iColors; i++)
   {
       pSymbols[i+SYM_OFFSETS] = iUncompressedLen + i; // root symbols
       pSymbols[i+SYM_LENGTHS] = 1;
       buf[iUncompressedLen + i] = (unsigned char) i;
   }
   memset(&pSymbols[iColors + SYM_LENGTHS], 0, (4096 - iColors) * sizeof(uint32_t));
   memset(&pSymbols[iColors + SYM_OFFSETS], 0xff, (4096-iColors) * sizeof(uint32_t));
   memset(&pSymbols[SYM_EXTRAS], 0xff, 4096 * sizeof(uint32_t));
   codesize = codestart + 1;
   sMask = -1 << (codestart+1);
   sMask = 0xffffffff - sMask;
   nextcode = cc + 2;
   nextlim = (1 << codesize);
   oldcode = code = (uint32_t)-1;
   while (code != eoi && iOffset < iUncompressedLen) /* Loop through all the data */
   {
       if (bitnum > (REGISTER_WIDTH - MAX_CODE_LEN)) // need to read more data
       {
           p += (bitnum >> 3);
           ulBits = INTELLONG(p); /* Read the next N-bit chunk */
           bitnum &= 7;
           ulBits >>= bitnum;
       }
       code = ulBits & sMask;
       ulBits >>= codesize;
       bitnum += codesize;
       
       if (code == cc) /* Clear code? */
       {
           if (oldcode == 0xffffffff) // no need to reset code table
               continue;
           else
               goto init_codetable;
       }
       if (code != eoi)
       {
           if (oldcode != -1)
           {
               if (nextcode < nextlim) // for deferred cc case, don't let it overwrite the last entry (fff)
               {
                   if (pSymbols[code] == -1) // new code
                   {
                       pSymbols[nextcode + SYM_LENGTHS] = LZWCopyBytes(buf, iOffset, iUncompressedLen, &pSymbols[oldcode]);
                       pSymbols[nextcode+SYM_OFFSETS] = iOffset;
                       c = buf[iOffset];
                       iOffset += pSymbols[nextcode+SYM_LENGTHS];
                       buf[iOffset++] = c; // repeat first character of old code on the end
                       pSymbols[nextcode+SYM_LENGTHS]++; // add to the length
                   }
                   else
                   {
                       iLen = LZWCopyBytes(buf, iOffset, iUncompressedLen, &pSymbols[code]);
                       pSymbols[nextcode+SYM_OFFSETS] = pSymbols[oldcode+SYM_OFFSETS];
                       pSymbols[nextcode+SYM_EXTRAS] = iOffset;
                       pSymbols[nextcode+SYM_LENGTHS] = pSymbols[oldcode+SYM_LENGTHS];
                       iOffset += iLen;
                   }
               }
               else // Deferred CC case - continue to use codes, but don't generate new ones
               {
                   iLen = LZWCopyBytes(buf, iOffset, iUncompressedLen, &pSymbols[code]);
                   iOffset += iLen;
               }
               nextcode++;
               if (nextcode >= nextlim && codesize < MAX_CODE_LEN)
               {
                   codesize++;
                   nextlim <<= 1;
                   sMask = (sMask << 1) | 1;
               }
           }
           else // first code
           {
               buf[iOffset++] = (unsigned char) code;
           }
           oldcode = code;
       } /* while not end of LZW code stream */
   }
    return iErr;
} /* DecodeLZW() */
//
// GifDeInterlace
//
void GifDeInterlace(SavedImage *pPage)
{
    int iGifPass = 0;
    int i, y;
    uint8_t *d, *s;
    uint8_t *pTemp = malloc(pPage->ImageDesc.Width * pPage->ImageDesc.Height);
    
    y = 0;
    for (i = 0; i < pPage->ImageDesc.Height; i++)
    {
        s = &pPage->RasterBits[i * pPage->ImageDesc.Width];
        d = &pTemp[y * pPage->ImageDesc.Width];
        memcpy(d, s, pPage->ImageDesc.Width);
        y += cGIFPass[iGifPass * 2];
        if (y >= pPage->ImageDesc.Height)
        {
            iGifPass++;
            y = cGIFPass[iGifPass * 2 + 1];
        }
    }
    memcpy(pPage->RasterBits, pTemp, pPage->ImageDesc.Width * pPage->ImageDesc.Height); // copy it back over source image
    free(pTemp);
} /* GIFDeInterlace() */

//
// GIFPreprocess
//
int GIFPreprocess(GifFileType *gif)
{
    int iOff;
//    int iDelay, iMaxDelay, iMinDelay, iTotalDelay;
    int iDataAvailable = 0;
    int bDone = 0;
    int bExt;
    uint8_t ucCodeStart, *pLZW;
    int iLZWSize;
    int i, iFrameMemCount;
    uint8_t c, *d, *cBuf, *pStart;
    SavedImage *pPage;
    GIFPRIVATE *pPrivate = gif->Private;
    ExtensionBlock *pExtensions;
    
//    iMaxDelay = iTotalDelay = 0;
//    iMinDelay = 10000;
    gif->ImageCount = 1;
    iFrameMemCount = GIF_IMAGE_INCREMENT; // how much memory we allocated
    pPage = gif->SavedImages = malloc(sizeof(SavedImage) * GIF_IMAGE_INCREMENT); // start by allocating N image structures
    cBuf = (uint8_t *) pPrivate->pFileData;
    iDataAvailable = pPrivate->iFileSize;
    iOff = 10;
    pPage->RasterBits = NULL; // no image data (yet)
    pPage->ImageDesc.ColorMap = NULL; // assume no palette (yet)
    c = cBuf[iOff];
    iOff += 3;   /* Skip flags, background color & aspect ratio */
    if (c & 0x80) /* Deal with global color table */
    {
        c &= 7;  /* Get the number of colors defined */
        iOff += (2<<c)*3; /* skip the global color table (we already got it) */
    }
    while (!bDone && gif->ImageCount < GIF_MAX_FRAMES)
    {
//        printf("DGifSlurp - frame %d\n", gif->ImageCount - 1);
        bExt = 1; // check for extension blocks
        pPage->ExtensionBlockCount = 0;
        // pre-allocate the max # of blocks since they're just a list of pointers/counts
        pPage->ExtensionBlocks = calloc(1, MAX_EXTENSIONS * sizeof(ExtensionBlock));
        pExtensions = pPage->ExtensionBlocks;
        while (bExt && iOff < iDataAvailable)
        {
            switch(cBuf[iOff])
            {
                case 0x3b: /* End of file */
                    /* we were fooled into thinking there were more pages */
                    gif->ImageCount--;
                    goto gifpagesz;
    // F9 = Graphic Control Extension (fixed length of 4 bytes)
    // FE = Comment Extension
    // FF = Application Extension
    // 01 = Plain Text Extension
                case 0x21: /* Extension block */
                    if (pPage->ExtensionBlockCount < MAX_EXTENSIONS) {
                        pExtensions[pPage->ExtensionBlockCount].Function = cBuf[iOff+1];
                        pExtensions[pPage->ExtensionBlockCount].ByteCount = cBuf[iOff+2];
                        pExtensions[pPage->ExtensionBlockCount].Bytes = &cBuf[iOff+3];
                        pPage->ExtensionBlockCount++;
                    }
#ifdef FUTURE
                    if (cBuf[iOff+1] == 0xf9 && cBuf[iOff+2] == 4) // Graphic Control Extension
                    {
                        // DEBUG!!!
                        pPage->ucHasExtension = 1;
                        pPage->ucGIFBits = cBuf[iOff+3]; // page disposition flags
                        pPage->iFrameDelay = cBuf[iOff+4]; // delay low byte
                        pPage->iFrameDelay |= (cBuf[iOff+5] << 8); // delay high byte
                        iDelay = pPage->iFrameDelay;
                        if (iDelay < 2) // too fast, provide a default
                            iDelay = 2;
                        iDelay *= 10; // turn JIFFIES into milliseconds
                        iTotalDelay += iDelay;
                        if (iDelay > iMaxDelay) iMaxDelay = iDelay;
                        else if (iDelay < iMinDelay) iMinDelay = iDelay;
                        pPage->ucTransparent = cBuf[iOff+6]; // transparent color index
                        printf("ucGIFBits: 0x%02x, ucTrans: 0x%02x\n", pPage->ucGIFBits, pPage->ucTransparent);
                    }
#endif
                    iOff += 2; /* skip to length */
                    iOff += (int)cBuf[iOff]; /* Skip the data block */
                    iOff++;
                   // block terminator or optional sub blocks
                    c = cBuf[iOff++]; /* Skip any sub-blocks */
                    while (c && iOff < (iDataAvailable - c))
                    {
                        if (pPage->ExtensionBlockCount < MAX_EXTENSIONS) {
                            pExtensions[pPage->ExtensionBlockCount].Function = 0; // 0 indicates more data for the previously defined extension
                            pExtensions[pPage->ExtensionBlockCount].ByteCount = c;
                            pExtensions[pPage->ExtensionBlockCount].Bytes = &cBuf[iOff];
                            pPage->ExtensionBlockCount++;
                        }
                        iOff += (int)c;
                        c = cBuf[iOff++];
                    }
                    if (c != 0) // problem, we went past the end
                    {
                        gif->ImageCount--; // possible corrupt data; stop
                        goto gifpagesz;
                    }
                    break;
                case 0x2c: /* Start of image data */
                    bExt = 0; /* Stop doing extension blocks */
                    break;
                default:
                   /* Corrupt data, stop here */
                    gif->ImageCount--;
                    goto gifpagesz;
            } // switch
        } // while
        if (cBuf[iOff] == ',')
            iOff++;
        if (iOff >= iDataAvailable) // problem
        {
             gif->ImageCount--; // possible corrupt data; stop
             goto gifpagesz;
        }
        /* Start of image data */
    // This particular frame's size and position on the main frame (if animated)
        pPage->ImageDesc.Left = INTELSHORT(&cBuf[iOff]);
        pPage->ImageDesc.Top = INTELSHORT(&cBuf[iOff+2]);
        pPage->ImageDesc.Width = INTELSHORT(&cBuf[iOff+4]);
        pPage->ImageDesc.Height = INTELSHORT(&cBuf[iOff+6]);
        iOff += 8;
        /* Image descriptor
         7 6 5 4 3 2 1 0    M=0 - use global color map, ignore pixel
         M I 0 0 0 pixel    M=1 - local color map follows, use pixel
         I=0 - Image in sequential order
         I=1 - Image in interlaced order
         pixel+1 = # bits per pixel for this image
         */
        c = cBuf[iOff++]; /* Get the flags byte */
        pPage->ImageDesc.Interlace = c & 0x40;
        if (c & 0x80) /* Local color table */
        {
            pPage->ImageDesc.ColorMap = (ColorMapObject *)malloc(sizeof(ColorMapObject));
            pPage->ImageDesc.ColorMap->ColorCount = (2<<(c & 7));
            pPage->ImageDesc.ColorMap->Colors = (GifColorType *)&cBuf[iOff];
            iOff += pPage->ImageDesc.ColorMap->ColorCount*3;
        }
        ucCodeStart = cBuf[iOff++]; /* LZW code size byte */
        c = cBuf[iOff++]; // first chunk length
        pLZW = &cBuf[iOff-1]; // start of compressed data
        // remove the chunk markers to make contiguous data
        d = &cBuf[iOff-1];
        pStart = d;
        while (c) /* While there are more data blocks */
        {
            memmove(d, &cBuf[iOff], c);
            d += c;
            iOff += c;
            c = cBuf[iOff++]; /* Get length of next */
        }
        iLZWSize = (int)(d - pStart);
//        printf(" - compressed size: %d\n", iLZWSize);
        /* End of image data, decode it */
        i = (pPage->ImageDesc.Width * pPage->ImageDesc.Height) + 8;
        i += 0xffff; // memory seems to fragment if we allocate many blocks
        i &= 0xffff0000; // of odd sizes, so round them to 64K
        pPage->RasterBits = malloc(i);
        // DEBUG - check for null
        if (DecodeLZW(gif, pPage, ucCodeStart, pLZW, iLZWSize) != GIF_OK) {
            // ERROR
        }
        if (pPage->ImageDesc.Interlace)
            GifDeInterlace(pPage);
        /* Check for more frames... */
        if (iOff >= pPrivate->iFileSize || cBuf[iOff] == 0x3b)
        {
            bDone = 1; /* End of file has been reached */
        }
        else /* More pages to scan */
        {
            gif->ImageCount++;
            if (gif->ImageCount >= iFrameMemCount) { // need to allocate more memory
                iFrameMemCount += GIF_IMAGE_INCREMENT;
                gif->SavedImages = realloc(gif->SavedImages, iFrameMemCount * sizeof(SavedImage));
            }
            pPage = &gif->SavedImages[gif->ImageCount-1];
            memset(pPage, 0, sizeof(SavedImage));
        }
    } /* while !bDone */
gifpagesz:
//    pPage = &gif->SavedImages[gif->ImageCount];
//    if (pPage->ExtensionBlocks)
//        free(pPage->ExtensionBlocks); // we allocated one too many
    return GIF_OK;
} /* GIFPreProcess() */

//
// DGifOpenFileHandle
//
GifFileType *DGifOpenFileHandle(int iHandle, int *pError)
{
GifFileType *gif;
GIFPRIVATE *pPrivate = NULL;
unsigned char ucTemp[32], SortFlag, BitsPerPixel;
int i;
    
    gif = (GifFileType *)calloc(1, sizeof(GifFileType));
    if (gif == NULL) {
        if (pError != NULL)
            *pError = D_GIF_ERR_NOT_ENOUGH_MEM;
        goto open_error;
        return NULL;
    }
    
    pPrivate = gif->Private = calloc(1, sizeof(GIFPRIVATE));
    if (gif->Private == NULL) {
        if (pError) {
            *pError = D_GIF_ERR_NOT_ENOUGH_MEM;
            goto open_error;
        }
    }
    pPrivate->iHandle = iHandle; // save for later
    
    // Read a bit of the file to get the signature and image descriptor
    i = (int)read(iHandle, ucTemp, 13);
    if (i != 13) {
        if (pError != NULL)
            *pError = D_GIF_ERR_READ_FAILED;
        goto open_error;
    }
    // See if it's a GIF file
    if (memcmp(ucTemp, GIF87_STAMP, GIF_STAMP_LEN) != 0 && memcmp(ucTemp, GIF89_STAMP, GIF_STAMP_LEN) != 0) {
        // not a GIF file
        if (pError != NULL)
            *pError = D_GIF_ERR_NOT_GIF_FILE;
        goto open_error;
    }
    // Get logical screen descriptor
    gif->SWidth = INTELSHORT(&ucTemp[6]);
    gif->SHeight = INTELSHORT(&ucTemp[8]);
    gif->SColorResolution = (((ucTemp[10] & 0x70) + 1) >> 4) + 1;
    SortFlag = ucTemp[10] & 0x8;
    BitsPerPixel = (ucTemp[10] & 7) + 1;
    gif->SBackGroundColor = ucTemp[11];
    gif->AspectByte = ucTemp[12];
    gif->SColorMap = NULL;
    if (ucTemp[10] & 0x80) { // global color table
        gif->SColorMap = GifMakeMapObject(1 << BitsPerPixel, NULL);
        if (gif->SColorMap == NULL) {
            if (pError != NULL)
                *pError = D_GIF_ERR_NOT_ENOUGH_MEM;
            goto open_error;
        }
        gif->SColorMap->SortFlag = SortFlag;
        // Read the palette entries
        i = (int)read(iHandle, gif->SColorMap->Colors, (1 << BitsPerPixel) * 3);
        if (i != (1 << BitsPerPixel) * 3) {
            if (pError != NULL)
                *pError = D_GIF_ERR_READ_FAILED;
            goto open_error;
        }
    }
    return gif;
    
open_error:
    (void)close(iHandle);
    if (gif) {
        if (gif->Private)
            free(gif->Private);
        if (gif->SColorMap) {
            GifFreeMapObject(gif->SColorMap);
        }
        free(gif);
    }
    return NULL;
} /* DGifOpenFileHandle() */

//
// DGifOpenFileName
//
GifFileType *DGifOpenFileName(const char *fname, int *pError)
{
int iHandle;

   iHandle = open(fname, O_RDONLY);
   if (iHandle == -1) {
       if (pError != NULL)
           *pError = D_GIF_ERR_OPEN_FAILED;
       return NULL;
   }
   return DGifOpenFileHandle(iHandle, pError);
} /* DGifOpenFileName() */
//
// DGifSlurp
//
// Read every frame of a GIF file into a list of SavedImage structures
//
int DGifSlurp(GifFileType * gif)
{
    GIFPRIVATE *pPrivate = NULL;
    int err = GIF_OK;
    unsigned char *p;

    if (gif == NULL || gif->Private == NULL)
        return D_GIF_ERR_READ_FAILED;
    pPrivate = (GIFPRIVATE *)gif->Private;
    if (pPrivate->iHandle <= 0)
        return D_GIF_ERR_NOT_READABLE;
    
    gif->ExtensionBlocks = NULL;
    gif->ExtensionBlockCount = 0;
    
    // Read the file data all at once. This will use a lot more RAM
    // but the gain in speed is significant vs reading it in small chunks
    pPrivate->iFileSize = (int)lseek(pPrivate->iHandle, 0, SEEK_END);
    lseek(pPrivate->iHandle, 0, SEEK_SET);
    if (pPrivate->iFileSize > 0) {
        pPrivate->pSymbols = malloc(3 * 4096 * sizeof(uint32_t)); // symbol memory
        if (pPrivate->pSymbols == NULL) {
            err = D_GIF_ERR_NOT_ENOUGH_MEM;
            goto slurp_error;
        }
        p = pPrivate->pFileData = (unsigned char *)malloc(pPrivate->iFileSize);
        if (pPrivate->pFileData == NULL) {
            err = D_GIF_ERR_NOT_ENOUGH_MEM;
            goto slurp_error;
        }
        read(pPrivate->iHandle, pPrivate->pFileData, pPrivate->iFileSize); // read the whole file into memory
        close(pPrivate->iHandle);
    }
    // Scan the file for images and collect the info
    GIFPreprocess(gif);
    // Decode all of the frames
slurp_error:
    return err;
} /* DGifSlurp() */

//
// DGifOpen
//
GifFileType *DGifOpen(void *userPtr, InputFunc readFunc, int *Error)
{
    return NULL;
} /* DGifOpen() */

//
// GifFreeImages
//
void GifFreeImages(GifFileType *gif) {
    if (gif != NULL) {
        for (int i=0; i<gif->ImageCount; i++) {
            SavedImage *pSI = &gif->SavedImages[i];
            if (pSI->RasterBits)
                free(pSI->RasterBits);
            if (pSI->ExtensionBlocks)
                free(pSI->ExtensionBlocks);
            if (pSI->ImageDesc.ColorMap) {
                free(pSI->ImageDesc.ColorMap);
            }
        }
        free(gif->SavedImages);
    }
} /* GifFreeImages() */
//
// DGifCloseFile
//
int DGifCloseFile(GifFileType * gif, int *ErrorCode)
{
    if (gif != NULL) {
        if (gif->Private) {
            GIFPRIVATE *pPrivate = (GIFPRIVATE *)gif->Private;
            if (pPrivate->iHandle)
                close(pPrivate->iHandle);
            if (pPrivate->pFileData)
                free(pPrivate->pFileData);
            if (pPrivate->pSymbols)
                free(pPrivate->pSymbols);
            free(gif->Private);
        }
        if (gif->SColorMap) {
            if (gif->SColorMap->Colors)
                free(gif->SColorMap->Colors); // the only color table explicitly allocated
            free(gif->SColorMap);
        }
        if (gif->SavedImages) {
            GifFreeImages(gif);
        }
        free(gif);
        return D_GIF_SUCCEEDED;
    }
    return D_GIF_ERR_CLOSE_FAILED;
} /* DGifCloseFile() */

