//
// GIFLIB test
//
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gif_lib.h"

void DoGifWedge(char *outname)
{
#define DEFAULT_WIDTH    640
#define DEFAULT_HEIGHT    350

#define DEFAULT_NUM_LEVELS    16     /* Number of colors to gen the image. */
    static int
        NumLevels = DEFAULT_NUM_LEVELS,
        ImageWidth = DEFAULT_WIDTH,
        ImageHeight = DEFAULT_HEIGHT;

    int    i, j, l, c, LevelStep, LogNumLevels, ErrorCode;
// int Count = 0;
  //  bool Error, LevelsFlag = false, SizeFlag = false, HelpFlag = false;
    GifRowType Line;
    ColorMapObject *ColorMap;
    GifFileType *GifFile;

    /* Make sure the number of levels is power of 2 (up to 32 levels.). */
    for (i = 1; i < 6; i++) if (NumLevels == (1 << i)) break;
//    if (i == 6) GIF_EXIT("#Lvls (-l option) is not power of 2 up to 32.");
    LogNumLevels = i + 3;               /* Multiple by 8 (see below). */
    LevelStep = 256 / NumLevels;

    /* Make sure the image dimension is a multiple of NumLevels horizontally */
    /* and 7 (White, Red, Green, Blue and Yellow Cyan Magenta) vertically.   */
    ImageWidth = (ImageWidth / NumLevels) * NumLevels;
    ImageHeight = (ImageHeight / 7) * 7;

    /* Open stdout for the output file: */
    if ((GifFile = EGifOpenFileName(outname, 0, &ErrorCode)) == NULL) {
//    if ((GifFile = EGifOpenFileHandle(1, &ErrorCode)) == NULL) {
//    PrintGifError(ErrorCode);
    exit(EXIT_FAILURE);
    }

    /* Dump out screen description with given size and generated color map:  */
    /* The color map has 7 NumLevels colors for White, Red, Green and then   */
    /* The secondary colors Yellow Cyan and magenta.                 */
    if ((ColorMap = GifMakeMapObject(8 * NumLevels, NULL)) == NULL)
        exit(EXIT_FAILURE);
//    GIF_EXIT("Failed to allocate memory required, aborted.");

    for (i = 0; i < 8; i++)                   /* Set color map. */
    for (j = 0; j < NumLevels; j++) {
        l = LevelStep * j;
        c = i * NumLevels + j;
        ColorMap->Colors[c].Red = (i == 0 || i == 1 || i == 4 || i == 6) * l;
        ColorMap->Colors[c].Green =    (i == 0 || i == 2 || i == 4 || i == 5) * l;
        ColorMap->Colors[c].Blue = (i == 0 || i == 3 || i == 5 || i == 6) * l;
    }

    if (EGifPutScreenDesc(GifFile, ImageWidth, ImageHeight, LogNumLevels, 0, ColorMap) == GIF_ERROR) {
//    PrintGifError(GifFile->Error);
    }

    /* Dump out the image descriptor: */
    if (EGifPutImageDesc(GifFile,
             0, 0, ImageWidth, ImageHeight,
             false, NULL) == GIF_ERROR) {

//    PrintGifError(GifFile->Error);
    exit(EXIT_FAILURE);
    }
    /* Allocate one scan line to be used for all image.                 */
    if ((Line = (GifRowType) malloc(sizeof(GifPixelType) * ImageWidth)) == NULL)
        exit(EXIT_FAILURE);
//    GIF_EXIT("Failed to allocate memory required, aborted.");

    /* Dump the pixels: */
    for (c = 0; c < 7; c++) {
    for (i = 0, l = 0; i < NumLevels; i++)
        for (j = 0; j < ImageWidth / NumLevels; j++)
        Line[l++] = i + NumLevels * c;
    for (i = 0; i < ImageHeight / 7; i++) {
        if (EGifPutLine(GifFile, Line, ImageWidth) == GIF_ERROR) {
//        PrintGifError(GifFile->Error);
        exit(EXIT_FAILURE);
        }
//        GifQprintf("\b\b\b\b%-4d", Count++);
    }
    }

    if (EGifCloseFile(GifFile, &ErrorCode) == GIF_ERROR) {
//    PrintGifError(ErrorCode);
    exit(EXIT_FAILURE);
    }

} /* DoGifWedge() */

int main(int argc, char **argv)
{
GifFileType *gif_in = NULL, *gif_out = NULL;
//GifImageDesc image_desc;
//GifRecordType type;
int gif_error = GIF_ERROR;
int i, j;
    
    DoGifWedge(argv[1]);
    return 0;
    
    if (argc != 2 && argc != 3) {
        printf("usage: gif_test <gif in file> <gif out file>\n");
        return 0;
    }
    for (i=0; i<100; i++) {
        gif_in = DGifOpenFileName(argv[1], &gif_error);
        if (gif_in != NULL) {
        DGifSlurp(gif_in);
        // Re-compress and write the same data out
        gif_out = EGifOpenFileName(argv[2], 0, &gif_error);
        // Copy the data into the output structure
        gif_out->SWidth = gif_in->SWidth;
        gif_out->SHeight = gif_in->SHeight;
        gif_out->SColorResolution = gif_in->SColorResolution;
        gif_out->SBackGroundColor = gif_in->SBackGroundColor;
        if (gif_in->SColorMap) {
                gif_out->SColorMap = GifMakeMapObject(gif_in->SColorMap->ColorCount, gif_in->SColorMap->Colors);
        } else {
            gif_out->SColorMap = NULL;
        }
        for (j = 0; j < gif_in->ImageCount; j++)
            (void) GifMakeSavedImage(gif_out, &gif_in->SavedImages[j]);

        EGifSpew(gif_out); // closes file and frees resources
        DGifCloseFile(gif_in, &gif_error);
	} else {
		printf("Error opening %s\n", argv[1]);
		return 0;
        }
  }
  return 0;
} /* main() */

