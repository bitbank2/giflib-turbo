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
#ifndef _GIF_LIB_H_
#define _GIF_LIB_H_ 

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GIFLIB_MAJOR 5
#define GIFLIB_MINOR 2
#define GIFLIB_RELEASE 1

#define GIF_ERROR   0
#define GIF_OK      1

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define INTELSHORT(p) ((*p) + (*(p+1)<<8))
#if (INTPTR_MAX == INT64_MAX)
#define INTELLONG(p) (*(uint64_t *)p)
#define REGISTER_WIDTH 64
#define BIGINT int64_t
#define BIGUINT uint64_t
#else
// Assume on a 32-bit CPU that unaligned reads/writes cause an exception
// This is true of most Arm Linux machines
#define INTELLONG(p) ((*p) + (*(p+1)<<8) + (*(p+2)<<16) + (*(p+3)<<24))
//#define INTELLONG(p) (*(uint32_t *)p)
#define REGISTER_WIDTH 32
#define BIGINT int32_t
#define BIGUINT uint32_t
#endif

#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif

#define SYM_OFFSETS 0x0000
#define SYM_LENGTHS 0x1000
#define SYM_EXTRAS  0x2000
#define MAX_CODE_LEN 12
#define MAX_HASH 5003
#define MAXMAXCODE 4096

#ifndef MAX
#define MAX(a, b) (a > b) ? a : b
#endif

#define GIF_STAMP "GIFVER"          /* First chars in file - GIF stamp.  */
#define GIF_STAMP_LEN sizeof(GIF_STAMP) - 1
#define GIF_VERSION_POS 3           /* Version first character in stamp. */
#define GIF87_STAMP "GIF87a"        /* First chars in file - GIF stamp.  */
#define GIF89_STAMP "GIF89a"        /* First chars in file - GIF stamp.  */

// Number of frames of image structures to be allocated at a time
#define GIF_IMAGE_INCREMENT 100
// Arbitrary number to keep it from getting stuck in an infinite loop
#define GIF_MAX_FRAMES 20000

typedef unsigned char GifPixelType;
typedef unsigned char *GifRowType;
typedef unsigned char GifByteType;
typedef unsigned int GifPrefixType;
typedef int GifWord;

typedef struct GifColorType {
    GifByteType Red, Green, Blue;
} GifColorType;

typedef struct ColorMapObject {
    int ColorCount;
    int BitsPerPixel;
    bool SortFlag;
    GifColorType *Colors;    /* on malloc(3) heap */
} ColorMapObject;

typedef struct GifImageDesc {
    GifWord Left, Top, Width, Height;   /* Current image dimensions. */
    bool Interlace;                     /* Sequential/Interlaced lines. */
    ColorMapObject *ColorMap;           /* The local color map */
} GifImageDesc;

#define MAX_EXTENSIONS 32
typedef struct ExtensionBlock {
    int ByteCount;
    GifByteType *Bytes; /* on malloc(3) heap */
    int Function;       /* The block function code */
#define CONTINUE_EXT_FUNC_CODE    0x00    /* continuation subblock */
#define COMMENT_EXT_FUNC_CODE     0xfe    /* comment */
#define GRAPHICS_EXT_FUNC_CODE    0xf9    /* graphics control (GIF89) */
#define PLAINTEXT_EXT_FUNC_CODE   0x01    /* plaintext */
#define APPLICATION_EXT_FUNC_CODE 0xff    /* application block (GIF89) */
} ExtensionBlock;

typedef struct SavedImage {
    GifImageDesc ImageDesc;
    GifByteType *RasterBits;         /* on malloc(3) heap */
    int ExtensionBlockCount;         /* Count of extensions before image */
    ExtensionBlock *ExtensionBlocks; /* Extensions before image */
} SavedImage;

typedef struct GifFileType {
    GifWord SWidth, SHeight;         /* Size of virtual canvas */
    GifWord SColorResolution;        /* How many colors can we generate? */
    GifWord SBackGroundColor;        /* Background color for virtual canvas */
    GifByteType AspectByte;          /* Used to compute pixel aspect ratio */
    ColorMapObject *SColorMap;       /* Global colormap, NULL if nonexistent. */
    int ImageCount;                  /* Number of current image (both APIs) */
    GifImageDesc Image;              /* Current image (low-level API) */
    SavedImage *SavedImages;         /* Image sequence (high-level API) */
    int ExtensionBlockCount;         /* Count extensions past last image */
    ExtensionBlock *ExtensionBlocks; /* Extensions past last image */
    int Error;                       /* Last error condition reported */
    void *UserData;                  /* hook to attach user data (TVT) */
    void *Private;                   /* Don't mess with this! */
} GifFileType;

typedef enum {
    UNDEFINED_RECORD_TYPE,
    SCREEN_DESC_RECORD_TYPE,
    IMAGE_DESC_RECORD_TYPE, /* Begin with ',' */
    EXTENSION_RECORD_TYPE,  /* Begin with '!' */
    TERMINATE_RECORD_TYPE   /* Begin with ';' */
} GifRecordType;

/* func type to read gif data from arbitrary sources (TVT) */
typedef int (*InputFunc) (GifFileType *, GifByteType *, int);

/* func type to write gif data to arbitrary targets.
 * Returns count of bytes written. (MRB)
 */
typedef int (*OutputFunc) (GifFileType *, const GifByteType *, int);

/******************************************************************************
 GIF89 structures
******************************************************************************/

typedef struct GraphicsControlBlock {
    int DisposalMode;
#define DISPOSAL_UNSPECIFIED      0       /* No disposal specified. */
#define DISPOSE_DO_NOT            1       /* Leave image in place */
#define DISPOSE_BACKGROUND        2       /* Set area too background color */
#define DISPOSE_PREVIOUS          3       /* Restore to previous content */
    bool UserInputFlag;      /* User confirmation required before disposal */
    int DelayTime;           /* pre-display delay in 0.01sec units */
    int TransparentColor;    /* Palette index for transparency, -1 if none */
#define NO_TRANSPARENT_COLOR    -1
} GraphicsControlBlock;

extern const char *GifErrorString(int ErrorCode);

// Decoder
GifFileType *DGifOpenFileName(const char *GifFileName, int *Error);
GifFileType *DGifOpenFileHandle(int GifFileHandle, int *Error);
int DGifSlurp(GifFileType * GifFile);
GifFileType *DGifOpen(void *userPtr, InputFunc readFunc, int *Error);    /* new one (TVT) */
    int DGifCloseFile(GifFileType * GifFile, int *ErrorCode);

// Encoder
GifFileType *EGifOpenFileName(const char *GifFileName, const bool GifTestExistence, int *Error);
GifFileType *EGifOpenFileHandle(const int GifFileHandle, int *Error);
GifFileType *EGifOpen(void *userPtr, OutputFunc writeFunc, int *Error);
int EGifSpew(GifFileType * GifFile);
const char *EGifGetGifVersion(GifFileType *GifFile); /* new in 5.x */
int EGifCloseFile(GifFileType *GifFile, int *ErrorCode);
void EGifSetGifVersion(GifFileType *GifFile, const bool gif89);
int EGifPutExtensionBlock(GifFileType *GifFile, const int ExtLen, const void *Extension);
int EGifPutExtensionTrailer(GifFileType *GifFile);
int EGifPutExtensionLeader(GifFileType *GifFile, const int ExtCode);
int EGifPutExtension(GifFileType *GifFile, const int ExtCode, const int ExtLen, const void *Extension);
int EGifPutScreenDesc(GifFileType *GifFile, const int Width, const int Height, const int ColorRes, const int BackGround, const ColorMapObject *ColorMap);
int EGifPutImageDesc(GifFileType *GifFile, const int Left, const int Top, const int Width, const int Height, const bool Interlace, const ColorMapObject *ColorMap);
int EGifPutLine(GifFileType *GifFile, GifPixelType *GifLine,
                int GifLineLen);
int EGifPutPixel(GifFileType *GifFile, GifPixelType Pixel);

// Common
ColorMapObject *GifMakeMapObject(int ColorCount, const GifColorType *ColorMap);
ColorMapObject *GifUnionColorMap(const ColorMapObject *ColorIn1,
                                     const ColorMapObject *ColorIn2,
                                     GifPixelType ColorTransIn2[]);
void GifFreeMapObject(ColorMapObject *Object);
SavedImage *GifMakeSavedImage(GifFileType *GifFile,
                                  const SavedImage *CopyFrom);
void GifFreeSavedImages(GifFileType *GifFile);
int DGifExtensionToGCB(const size_t GifExtensionLength,
               const GifByteType *GifExtension,
               GraphicsControlBlock *GCB);
size_t EGifGCBToExtension(const GraphicsControlBlock *GCB,
               GifByteType *GifExtension);

int DGifSavedExtensionToGCB(GifFileType *GifFile,
                int ImageIndex,
                GraphicsControlBlock *GCB);
int EGifGCBToSavedExtension(const GraphicsControlBlock *GCB,
                GifFileType *GifFile,
                int ImageIndex);

// Decoder error constants
#define D_GIF_SUCCEEDED          0
#define D_GIF_ERR_OPEN_FAILED    101
#define D_GIF_ERR_READ_FAILED    102
#define D_GIF_ERR_NOT_GIF_FILE   103
#define D_GIF_ERR_NO_SCRN_DSCR   104
#define D_GIF_ERR_NO_IMAG_DSCR   105
#define D_GIF_ERR_NO_COLOR_MAP   106
#define D_GIF_ERR_WRONG_RECORD   107
#define D_GIF_ERR_DATA_TOO_BIG   108
#define D_GIF_ERR_NOT_ENOUGH_MEM 109
#define D_GIF_ERR_CLOSE_FAILED   110
#define D_GIF_ERR_NOT_READABLE   111
#define D_GIF_ERR_IMAGE_DEFECT   112
#define D_GIF_ERR_EOF_TOO_SOON   113

// Encoder error constants
#define E_GIF_SUCCEEDED          0
#define E_GIF_ERR_OPEN_FAILED    1
#define E_GIF_ERR_WRITE_FAILED   2
#define E_GIF_ERR_HAS_SCRN_DSCR  3
#define E_GIF_ERR_HAS_IMAG_DSCR  4
#define E_GIF_ERR_NO_COLOR_MAP   5
#define E_GIF_ERR_DATA_TOO_BIG   6
#define E_GIF_ERR_NOT_ENOUGH_MEM 7
#define E_GIF_ERR_DISK_IS_FULL   8
#define E_GIF_ERR_CLOSE_FAILED   9
#define E_GIF_ERR_NOT_WRITEABLE  10

//======= Private Structures =======

typedef struct gif_private
{
    uint32_t *pSymbols; // temp memory for encode/decode
    int iHandle; // file handle
    int iFileSize;
    int iBitsPerPixel;
    int iPixelCount; // pixels remaining in current image being written
    unsigned char *pFileData;
} GIFPRIVATE;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _GIF_LIB_H */

