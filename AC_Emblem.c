/*
Armored-Core Emblem Implementer v0.2

Copyright (c) 2019 Anatoly Lerner

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
 
#define STBI_NO_JPEG
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_LINEAR
#define STBI_NO_HDR  // Exclude unused methods

#define STBI_FAILURE_USERMSG  // Enable user friendly messages

#define STB_IMAGE_IMPLEMENTATION // Enable this library
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION // Enable this library
#include "stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION // Enable this library
#include "stb_image_resize.h"

#include "exoquant.h" // image quantizer

#define UCHAR unsigned char

void FormatPath(char Str[]){
	if(*Str == '"'){ 
		strcpy(Str, Str+1);
		*(Str + strlen(Str) - 1) = '\0';
	}
}

void TerminateProgram(char str[]){
	printf("%s%s%s", "\nError: ", str, " not found. Terminating.\nPress any key to exit...");
	getchar();
	exit(1);
}

int TransparencyTest(UCHAR *img, int width, int height){
	int i, r = 0;
	for(i = 0; i < width*height; i++){
		if(*(img + i*4 + 3) < 0x80){
			// binary transparency
			*(img + i*4 + 3) = 0x00;
			// transparent_pixel_found flag
			r = 1;
		}
		// binary transparency
		else{ *(img + i*4 + 3) = 0xff;}
	}
	return r;
}

void ImageInput(UCHAR **img){
	char filename[100]; int i, width, height, channels;
	
	puts("Drag and drop your image file into this window (or enter the full path).");
	puts("Requirements:\n   Format: BMP or PNG (transparency is supported)\n   Dimensions: 128 by 128 pixels");
	do{
		i = 0;
		printf("%s","File: "); gets(filename);
		FormatPath(filename);
		*img = stbi_load(filename, &width, &height, &channels, 4);
		if(*img == NULL){
			printf("%s%s%s\n", "   Error: ", stbi_failure_reason(), ". Please try again."); 
			i = 1;
		}
		else if(width != 128 || height != 128){
			printf("%s\n", "   Error: Unsupported image dimensions. Please try again.");
			stbi_image_free(*img);
			i = 1;
		}
	}while(i != 0);
}

void QuantizeImage(UCHAR *img, UCHAR **img_idx, UCHAR **img_pal){
	
	*img_pal = (UCHAR*)malloc(sizeof(UCHAR)*256*4);
	*img_idx = (UCHAR*)malloc(sizeof(UCHAR)*128*128);
	
	// check for transparency and quantize it
	int trn = TransparencyTest(img, 128, 128);
	
	exq_data *pExq = exq_init(); // initialize the quantizer (per image)
	exq_feed(pExq, img, 128*128); // feed pixel data (32bpp)
	
	// Armored-Core palette has 256 elements:  transparency and 255 colors
    // The "exq_quantize" function will treat transparency as one of the colors.
	// Therefore a correction is needed if transparency is present in the image.
	// This transparent color, if it exists, is guaranteed to be the first one in the "exq_quantize" palette.
	exq_quantize(pExq, 255+trn); // find palette
	exq_get_palette(pExq, (*img_pal)+4*(1-trn), 255+trn); // get palette (offset)
	
	*(*img_pal) = *((*img_pal)+1) = *((*img_pal)+2) = 0x00; *((*img_pal)+3) = 0xff;
	
	exq_map_image(pExq, 128*128, img, *img_idx);
	
	if(trn == 0){
		for(int i = 0; i < 128*128; i++){
			*(*img_idx + i) = *(*img_idx + i) + 1;
		}
	}
	
	exq_free(pExq);

}

void GenerateSavedata(UCHAR *img_idx, UCHAR *img_pal){
	FILE *fin, *fout;
	int i, j;
	UCHAR KeyBuff[1024];
	
	fin = fopen("RESOURCES/SUBKEYS.BIN", "rb");
	if(fin == NULL){ TerminateProgram("\"RESOURCES/SUBKEYS.BIN\" file");}
	
	fout = fopen("NPUH10023AC3EMBLEMLIST00/SAVEDATA.BIN", "wb");
	if(fout == NULL){ TerminateProgram("\"NPUH10023AC3EMBLEMLIST00\" folder");}
	
	// Copy over the first 48 bytes, they contain the main key and other info.
	fread(KeyBuff, 1, 48, fin);
	fwrite(KeyBuff, 1, 48, fout);
	
	// Imbed the bitmap, 1 byte per pixel
	for(i = 0; i < 16; i++){ // steps needed to embed the whole image (1024 bytes at a time): (128*128)/1024 = 16
		fread(KeyBuff, 1, 1024, fin);
		for(j = 0; j < 1024; j++){ // 8 rows per 1024 pixels
			KeyBuff[j] = KeyBuff[j] ^ (*img_idx); // XOR mask (Encryption)
			img_idx++;
		}
		fwrite(KeyBuff, 1, 1024, fout);
	}
	
	
	// Writing the palette, 256 entries, 4 bytes per entry
	fread(KeyBuff, 1, 1024, fin);
	for(i = j = 0; i < 256; i++, j+=4){
		KeyBuff[ j ] = KeyBuff[ j ] ^ *img_pal; img_pal++;
		KeyBuff[j+1] = KeyBuff[j+1] ^ *img_pal; img_pal++;
		KeyBuff[j+2] = KeyBuff[j+2] ^ *img_pal; img_pal++;
		KeyBuff[j+3] = KeyBuff[j+3]; img_pal++; // XOR mask (Encryption)
	}
	fwrite(KeyBuff, 1, 1024, fout);
	
	// Remaining bytes, simply copy them over
	fread(KeyBuff, 1, 992, fin);
	fwrite(KeyBuff, 1, 992, fout);
	
	fclose(fin);
	fclose(fout);
}

void GenerateIcon(UCHAR *img){
	
	UCHAR *icon; int width, height, channels;
	
	icon = stbi_load("RESOURCES/ICON.PNG", &width, &height, &channels, 4);
	if(*img == NULL){ TerminateProgram("\"RESOURCES/ICON.PNG\" file");}
	if(width != 144 || height != 80){
		puts("\nError: \"RESOURCES/ICON.PNG\" is corrupted. Terminating.\nPress any key to exit...");
		getchar(); exit(1);
	}
	
	// resize the given image
	UCHAR *tmp = (UCHAR*)malloc(sizeof(UCHAR)*78*78*4);
	stbir_resize_uint8(img , 128 , 128 , 0, tmp, 78, 78, 0, 4);
	
	 // binary transparency
	TransparencyTest(tmp, 78, 78);
	
	// merge images
	UCHAR *ui, *ut; int i, j, k;
	ut = tmp;
	for(i = 0; i < 78; i++){
		ui = icon + (144*(i+1) + 1)*4;
		for(j = 0; j < 78; j++){
			if(*(ut+3) != 0x00){
				for(k = 0; k < 4; k++){ *ui = *ut; ui++; ut++;}	
			}
			else{ ut += 4; ui += 4;}
		}
	}
	
	stbi_write_png("NPUH10023AC3EMBLEMLIST00/ICON0.PNG", 144, 80, 4, icon, 0);
	
	stbi_image_free(icon);
	free(tmp);
	
}

void CopyOtherFiles(){
	
	FILE *fin, *fout;
	int i, j;
	UCHAR KeyBuff[1024];
	
	fin = fopen("RESOURCES/PARAM.SFO", "rb");
	if(fin == NULL){ TerminateProgram("\"RESOURCES/PARAM.SFO\" file");}
	
	fout = fopen("NPUH10023AC3EMBLEMLIST00/PARAM.SFO", "wb");
	
	do{
		i = fread(KeyBuff, 1, 1024, fin);
		fwrite(KeyBuff, 1, i, fout);
	}while(i == 1024);
	
	fclose(fin);
	fclose(fout);
	
	fin = fopen("RESOURCES/PIC1.PNG", "rb");
	if(fin == NULL){ TerminateProgram("\"RESOURCES/PIC1.PNG\" file");}
	
	fout = fopen("NPUH10023AC3EMBLEMLIST00/PIC1.PNG", "wb");
	
	do{
		i = fread(KeyBuff, 1, 1024, fin);
		fwrite(KeyBuff, 1, i, fout);
	}while(i == 1024);
	
	fclose(fin);
	fclose(fout);
	
}

int main(){
	
	puts("=== Armored-Core Emblem Creator (PPSSPP) ===\n");
	
	UCHAR *img, *img_idx, *img_pal;
	
	ImageInput(&img);
	
	printf("\nProcessing...");
	
	QuantizeImage(img, &img_idx, &img_pal);
	
	GenerateSavedata(img_idx, img_pal);
	
	GenerateIcon(img);
	
	CopyOtherFiles();
			
	stbi_image_free(img);
	stbi_image_free(img_idx);
	stbi_image_free(img_pal);
	
	puts("Done.\n");
	puts("The \"NPUH10023AC3EMBLEMLIST00\" folder now contains all the necessary files.");
	puts("All you have to do now is copy-paste this folder into the \"\\memstick\\PSP\\SAVEDATA\\\" directory of your PPSSPP emulator.");
	puts("After that, lunch your Armored-core game (AC3, ACSL, ACLR or ACFF) and navigate to \"MAIN MENU -> SYSTEM -> LOAD EMBLEM\"");
	puts("You should be able to find your emblem there.");
	puts("\nHave fun :). Press any key to exit...");
	getchar();
	return 0;
}