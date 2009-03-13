/*
	DustCleaner is a GIMP plugin to detect and remove the dust spots 
	in digital image.
	Copyright (C) 2006-2007  Frank Tao<solotim.cn@gmail.com>

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
 * dustfinder.c 
 * Author: Frank.Tao
 * Date: 2006.10.11
 * Version: 0.1.01
 */

//#include <fftw3.h>  /* using FFTW */
#include <math.h>
#include "dust.h"



//int FFT_N = 1024; 		/* the size of fft transformation */

int dust_count = 0;

int initialSearch( const IplImage *image, CvSeq *fragSeq, CvSeq *blockSeq, TPara *para )
{
	int i; 
	// check para
	para->scanStep = intMax(1, para->scanStep); 
	// zero dust counter	
	dust_count = 0;
	// clear blockSeq's childSeq
	for (i=0; i<blockSeq->total; i++) {
		DustBlock* next = (DustBlock*)cvGetSeqElem( blockSeq, i );	
		freeDustBlock(next, 0);		
	}
	// clear blockSeq and fragSeq
	cvClearSeq(blockSeq);    
	cvClearSeq(fragSeq);	
	
	// initial compensation map
	for ( i=0; i<256; i++ ) {
		para->compensationMap[i] = atan( i/10 );
	}
    return i;
}

/* Search dust in the image and mark it
 * parameter:
 * IplImage *image: 1 channels image
 * TPara *para: search option
 * return: 
 * int: the number of dust spots 
 */
int searchDust( const IplImage *image, CvSeq *fragSeq, TPara *para )
{
	
	int i, j, m, n, 
		 k = 0; 
	int dustWidth = para->dustSize.width,
		 dustHeight = para->dustSize.height;
	int imgWidth = image->width,
		 imgHeight = image->height;
	int scanStep = para->scanStep;		
	CvMat* grayline = cvCreateMat( 1, image->width, CV_32SC1 );
	int* output = (int *)malloc(image->width * sizeof(int));
	float* convolution_core = produceCore(para->noiseReductionCoreLenght, 
																	para->noiseReductionCorePower);		

	// Convert to grayscale if necessary
	
	// START dust fragment searching loop		 
	for ( i=0; i<imgHeight; i=i+scanStep ){		
		int step;				
		memset(output, 0, image->width * sizeof(int));
		// Judge wether the scanning has beyond the image border
		step = i+scanStep>imgHeight ? imgHeight-i : scanStep; 			
		getAccumulatedGrayValue(i, step, image, grayline);		
#ifdef DEBUG       
			 writeData("output.txt", grayline, imgWidth);
#endif   		  

	/*	i=0;
	while (i++ < 340  ) {
		printf("out %d/%d: %f\n", i, imgWidth,output[i]);
	}
	cvWaitKey(0);   */
		int* raw;
		cvGetRawData( grayline, (uchar**)&raw, NULL, NULL );		
		t_convolution( raw,
						image->width, 
						&convolution_core[0], 
						para->noiseReductionCoreLenght, 
						output, 
						0 );
#ifdef DEBUG    	   	
			writeData("output1.txt", output, image->width);
#endif       

			 m = 0;
			 n = 0;       
			 // calculate the gray value difference
			 for ( j=0; j<image->width; j++ ) {       
				output[j] = raw[j] - output[j];
				if ( output[j]>0 ) {
					n += output[j];
					m++;
				}       	
			 }       
			 double avgNoise = n/((double)m);      // calculate average noise       
			 //printf( "avgNoise is %f , %d, %d\n", avgNoise, n, m );		
			 int threshold = -1 * avgNoise * (100 - para->sensitivity) / 10;
			 
		// START dust fragment recording loop     
			 for ( j=0; j<image->width; j++ ) {       	
				if ( output[j] < threshold) {  // dust fragment be found
					DustFragment frag;
					k = j;
					while ( output[k] < 0 ) {
						if ( k>0 )
							k--;
						else
							break;
					}       		
					frag.left = k;
					frag.row = i;
					frag.step = step;
					frag.flag = 0;
					
					k = j;
					while ( output[k] < 0 ) {
						if ( k<image->width-1 )
							k++;
						else
							break;
					}
					frag.right = k;
					j = k;              // Skip current dust fragment
					
					// check the width of dust fragment
					if ( frag.right - frag.left > para->dustSize.width ) {
						cvSeqPush( fragSeq, &frag );
#ifdef DEBUG   
						DustFragment* added = (DustFragment*)cvSeqPush( fragSeq, &frag );
                        printf( "frag (%d,  %d,  %d)  is added\n", added->row, added->left, added->right );				
#endif 			
					}else {
					
					}       								
				}       	
			 }   // END dust fragment recording loop     
	}  // END dust fragment searching loop		 	
	
	cvReleaseMat(&grayline); 	
	free(output); 
	free(convolution_core);	
	return 0;
}

/* Check two DustFragment for neighbor
 * parameter:
 * DustFragment* frag1: first dust fragment
 * DustFragment* frag2: second one
 * return: int
 *           1  :   yes, they are neighbor
 *           0  :   not neighbor, but close
 *          -1  :   far from neighbor 
 */
int isNeighbor(DustFragment* frag1, DustFragment* frag2)
{	
	int rt;	
	if ( abs(frag1->row - frag2->row) == intMax(frag1->step, frag2->step) ) {
		int max = intMax(frag1->right , frag2->right);
		int min = intMin(frag1->left , frag2->left);			
		if (frag1->right - frag1->left + frag2->right - frag2->left > max - min) {			
			rt = 1;
		}else{
			rt = 0;	
		}		
	}else if(abs(frag1->row - frag2->row) == 0){
		rt = 0;
	}else {
		rt = -1;
	}
	//rt=0; // test
	return rt;
}


/* Cluster dust fragments
 * parameter:
 * CvSeq *fragSeq: dust fragment sequence
 * int seed: seed fragment, position number of fragment sequence
 * DustBlock* pack: dust block pack
 * return: 
 * DustBlock*: a pointer refer to pack
 */
DustBlock* dustClustering ( CvSeq *fragSeq, int seed, DustBlock* pack, TPara* para ) 
{
	// if pack is new
	if ( pack->flag == 0 ) {
		// get seed fragment from pool
		DustFragment* seed_frag = (DustFragment*)cvGetSeqElem( fragSeq, seed );				
		// set current fragment to checked status
		seed_frag->flag = 1;  	
		// put current fragment to childFrag of pack		
		pack->childFrag = (LinkedFragVector*)malloc(sizeof(LinkedFragVector));
		pack->childFrag->dustFragment = seed_frag;
		pack->childFrag->next = NULL;
		// set pack values		
		pack->row0 = seed_frag->row;
		pack->col0 = seed_frag->left;
		pack->row1 = seed_frag->row + seed_frag->step -1;
		pack->col1 = seed_frag->right;
		pack->flag = 1;
		// find current fragment's neighbor
		dustClustering(fragSeq, seed, pack, para);		
	}else{		// else, searching new neighbor according to current pack
		// stitch seed fragment and its position		
		DustFragment* this_frag = (DustFragment*)cvGetSeqElem( fragSeq, seed );
		int current = seed;	
		// forward searching loop
		while ( 1 ) {			
			current++;
			// index checking
			if (current >= fragSeq->total ) break;			
			// get next dust fragment and compare it with current one				
			DustFragment* next_frag = (DustFragment*)cvGetSeqElem( fragSeq, current );			
			if (next_frag->flag == 1) continue;
			int isNb = isNeighbor(this_frag, next_frag);			
			if ( isNb == 1 ) {				// is neighbor, then add it to current pack				
				next_frag->flag = 1;
				LinkedFragVector* element = (LinkedFragVector*)malloc(sizeof(LinkedFragVector));
				element->dustFragment = next_frag;
				element->next = pack->childFrag;
				pack->childFrag = element;
				pack->row0 = intMin(next_frag->row, pack->row0);
				pack->col0 = intMin(next_frag->left, pack->col0);
				pack->row1 = intMax(next_frag->row + next_frag->step -1, pack->row1);
				pack->col1 = intMax(next_frag->right, pack->col1);
				dustClustering(fragSeq, current, pack, para);
			}else if( isNb == 0 ) {     // not neighbor, but close to current fragment, so continue to search next one
				continue;	
			}else {                          // not neighbor, and far from current fragment
				break;
			}			
		}	// END of forward searching loop
		current = seed;
		// backward searching loop
		while ( 1 ) {			// basically the same procedure with forward searching loop except direction
			current--;
			if (current < 0 ) break;
			DustFragment* next_frag = (DustFragment*)cvGetSeqElem( fragSeq, current );
			if (next_frag->flag == 1) continue;
			int isNb = isNeighbor(this_frag, next_frag);
			if ( isNb == 1 ) {
				next_frag->flag = 1;								
				LinkedFragVector* element = (LinkedFragVector*)malloc(sizeof(LinkedFragVector));
				element->dustFragment = next_frag;
				element->next = pack->childFrag;
				pack->childFrag = element;	
				pack->row0 = intMin(next_frag->row, pack->row0);
				pack->col0 = intMin(next_frag->left, pack->col0);
				pack->row1 = intMax(next_frag->row + next_frag->step -1, pack->row1);
				pack->col1 = intMax(next_frag->right, pack->col1);
				dustClustering(fragSeq, current, pack, para);
			}else if( isNb == 0 ) { 		// not neighbor, but close to current fragment, so continue to search next one
				continue;	
			}else {								// not neighbor, and far from current fragment
				break;
			}			
		}	// END of backward searching loop
		
	}
	return pack;				
}


/* Convert dust fragment sequence to dust block sequence, 
 * using dustClustering( CvSeq *fragSeq, int seed, DustBlock* pack )
 * parameter:
 * CvSeq *fragSeq: dust fragment sequence 
 * CvSeq *blockSeq: dust block sequence
 * return: 
 * int: 0
 */
int cvtFragToBlock( CvSeq *fragSeq, CvSeq *blockSeq, TPara* para)
{
	int i;	
	if (fragSeq->total <= 0) {
		printf( "--Dust Dection Finished! No dust has been detected.\n");	
		return 0;	
	}
	for ( i=0; i<fragSeq->total; i++ ) {		
		DustFragment *next;	
		next = (DustFragment*)cvGetSeqElem( fragSeq, i );
		
		if ( next->flag == 0 ){						
			// new pack
			DustBlock* pack = newDustBlock();			
			// clustering
			dustClustering(fragSeq, i, pack, para);		
			// check this pack
			if(dustCheck(pack, para)){
				// one pack created, then set pack's serial number
				pack->sn = ++dust_count;				
				cvSeqPush( blockSeq, pack );		
			}else	{			
				freeDustBlock(pack, 1);	
			}			
		}
	}
	printf( "--Dust Dection Finished! Totally %d dusts have been detected.\n", dust_count);	
	
	return 1;
}

/* Mark the dusts of given image with corresponding dust block sequence
 * parameter:
 * TPara *para: configuration parameters
 * CvSeq *blockSeq: dust block sequence, which will be updated with accurate dust area by this function
 * IplImage *markedImage: output marked image
 * return: 
 * int: 0
 */
int markDust( const IplImage *image, IplImage *markedImage, CvSeq *blockSeq, TPara *para)
{
	int i;
	char buffer[3];
	CvFont  font;
	if (blockSeq->total <= 0) return 0;
		
	cvInitFont( &font, CV_FONT_HERSHEY_PLAIN , 0.7f, 0.7f, 0, 1, 8 );

	for (i=0; i<blockSeq->total; i++) {
		DustBlock *next;				
		next = (DustBlock*)cvGetSeqElem( blockSeq, i );					
				
		// calculate the accurate dust area, 'next' will be updated
		findDustArea( image, next, para );				
		
		// draw rectangle, contour and serial on markedImage if it's not null
		if ( markedImage != NULL ) {
			CvPoint pt1, pt2;	
			pt1.x = next->col0;
			pt1.y = next->row0;
			pt2.x = next->col1;
			pt2.y = next->row1;
			cvRectangle( markedImage, pt1, pt2, CV_RGB(250, 1, 1), 1, 8, 0 );
			sprintf(buffer, "%d", next->sn); 
			cvPutText( markedImage, buffer, pt1, &font, CV_RGB(1,250,1));				
		
			// prepare to draw contours
			CvMemStorage* storage = cvCreateMemStorage(0);
			CvSeq* contour = 0;
			// find contours. since cvFindContours modifies the source image content, create a copy
			CvMat* colorOffset_copy = cvCloneMat(next->colorOffset); 
			cvFindContours( colorOffset_copy, storage, 
										&contour, sizeof(CvContour), 
										CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE,
										cvPoint(0,0) );
			// draw contours to markedImage
			for( ; contour != 0; contour = contour->h_next )
			{
				CvScalar color = CV_RGB( 200, 250, 120 );			
				cvDrawContours( markedImage, contour, color, color, 0, 
											1, 8, cvPoint(next->offsetX, next->offsetY) );
			}		
			cvReleaseMemStorage( &storage );		
			cvReleaseMat( &colorOffset_copy );
		}
		//printf( "i am ok!!! %d", i );
	}
	//cvSaveImage( "marked.jpg", markedImage );
	//printf( "--Marked image has been saved in current path, namely ./marked.jpg, please check it.\n");	
	return 0;
}

/* Record the dust area and their color offset
 * parameter:
 * IplImage *image: source image
 * CvSeq *blockSeq: dust block sequence of source image
 * TPara *para: configuration parameters
 * return: 
 * int: 1
 */
int recordDust( const IplImage *image, CvSeq *blockSeq, TPara *para ) {
	int i;
	
	if (blockSeq->total <= 0) return 0;
	
	for (i=0; i<blockSeq->total; i++) {
		DustBlock *next;				
		next = (DustBlock*)cvGetSeqElem( blockSeq, i );						
		distillColorOffset( image, next, para);
		// ToDo: wirte to file
		//
		//
		//
		//printf( "i am ok!!! %d", i );
	}	
	
	return 1;
}





