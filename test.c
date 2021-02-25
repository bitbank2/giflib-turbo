//
// GIFLIB test
//
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gif_lib.h"

int main(int argc, char **argv)
{
GifFileType *gif_in = NULL, *gif_out = NULL;
//GifImageDesc image_desc;
//GifRecordType type;
int gif_error = GIF_ERROR;
int i, j;
    
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

