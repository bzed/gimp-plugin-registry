/*
	DustCleaner is a GIMP plugin to detect and remove the dust spots 
	in digital image.
	Copyright (C) 2006-2008  Frank Tao<solotim.cn@gmail.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
	02110-1301, USA        	
*/
/*
 * DustCleaner Core Header File 
 * Author: Frank.Tao<solotim.cn@gmail.com>
 * Date: 2008.01.12
 * Version: 0.1.02
 */

#ifndef __DUST_H__
#define __DUST_H__
 
#include <cv.h>
#include <highgui.h>
#include <stdio.h>

#define DUST_DETECTION 1
#define DUST_RECOVERY_INSTANTLY 2
#define DUST_RECOVERY_BY_MASK 3

struct _TPara
{
    uchar **ptr;
    double compensationMap[256];
    CvSize dustSize;    
    int sensitivity; 			// 1-100
    int scanStep;
    int noiseReductionCoreLenght;
    int noiseReductionCorePower;
    int hasContourMorphologyEx;
    int kernelSize;
    int kernelType;
    int isSave;
    int op;
    int margin;				//-10-10
    int segmentationSensitivity;	// 1-100
    int recoveryStrength;
};

typedef struct _TPara TPara;

typedef struct 
{
    int row;
    int left;
    int right;    
    int step;
    int flag;
}
DustFragment;

typedef struct LinkedFragVector
{
   DustFragment* dustFragment;
   struct LinkedFragVector* next;
}
LinkedFragVector;

typedef struct 
{
    int row0;
    int col0;
    int row1;
    int col1;        
    LinkedFragVector* childFrag;   
    int offsetX;
    int offsetY;
    CvMat* colorOffset;
    int sn;
    char flag;
}
DustBlock;

// utilities.c
void writeData(char filename[], double *data, int size);
int getAccumulatedGrayValue( int row, int step, const IplImage *img, CvMat *data );
int t_convolution( const int *data, int count, float *core, int coreCount,  int *output, int TYPE );
float* produceCore( int size, float power );
int dustCheck(DustBlock* pack, TPara* para);
void freeLinkedFragVector(LinkedFragVector* p);
DustBlock*  newDustBlock();
void freeDustBlock(DustBlock* p, int suiside);
int intMax (int a, int b);
int intMin (int a, int b);

// dustfinder.c
int initialSearch( const IplImage *image, CvSeq *fragSeq, CvSeq *blockSeq, TPara *para );
int searchDust( const IplImage *image, CvSeq *fragSeq, TPara *para );
int isNeighbor(DustFragment* frag1, DustFragment* frag2);
DustBlock* dustClustering ( CvSeq *fragSeq, int seed, DustBlock* pack, TPara *para );
int cvtFragToBlock( CvSeq *fragSeq, CvSeq *blockSeq, TPara *para);
int markDust(  const  IplImage *image, IplImage *markedImage, CvSeq *blockSeq, TPara *para) ;
int recordDust( const IplImage *image, CvSeq *blockSeq, TPara *para );

// dustanalyzer.c
int findDustArea( const IplImage *image, DustBlock* dust, TPara *para );
int calcuThresholdAndSeed( CvArr* mat, DustBlock* dustBlk, CvPoint* seed );
CvMat* distillColorOffset( const IplImage *image, DustBlock* dust, TPara *para);
int mapping( const CvMat* src, const CvMat* offset, CvMat* dst, double* map, int isReverse );

// recovery.c
int recoveryImage( IplImage *img, CvSeq *blockSeq, TPara *para );


//CImageAccessStub.c
//int dustProcess(const char *filename, IplImage *rstImg, CvRect cropRect, TPara *_para);
//void dustProcess(const char *filename1, const char *filename2, int *element, short* result);

#endif

