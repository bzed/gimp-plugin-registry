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
 * Utilities 
 * Author: Frank.Tao<solotim.cn@gmail.com>
 * Date: 2006.10.02
 * Version: 0.1.01
 */

#include <stdio.h>
#include "dust.h"

/* Get the Accumulated Gray Value
 * parameter:
 * int row: the start row of img
 * int step: how many rows to be accumulated
 * IplImage *img: the source 1 channel 8 bit image
 * CvMat *data: the output values, orgnized by one row
 */
int getAccumulatedGrayValue( int row, int step, const IplImage *img, CvMat *data ) {
	int i, j;
	CvRect rect;
	CvMat ma;
	
	rect = cvRect( 0, row, img->width, step );	
	cvGetSubRect( img, &ma, rect );		
	cvReduce( &ma, data, 0, CV_REDUCE_SUM );			
	return 0;
}

/* A simple convolution function
 * parameter:
 * const int *data: the data to be convoluted
 * int count: the count of data
 * float *core: the convolution core
 * int coreCount: the count of convolution core
 * int *output: the output
 * int TYPE: convolution type, @todo:
 */
int t_convolution( const int *data, int count, float *core, int coreCount,  int *output, int TYPE ){
	int i, j, wing;
	float tmp, s;	
	
	wing = (coreCount-1)/2;
	for (i=0; i<count; i++) {		
		for (j=0, s = 0.0f; j<coreCount; j++){
			int out = i+j-wing;
			if (out < 0)
				tmp = data[-1 * out];
			else if (out>count-1)
				tmp = data[2*count-2-out];
			else
				tmp = data[i+j-wing];									  			
			s += tmp * core[j];			
		}		
		output[i] = (int)s;
	}
	return 0;
}

/* Create a convolution core
 * parameter:
 * int size: the size of the core
 * float power: power of the core
 * return float*: a pointer refer to the core
 */
float* produceCore( int size, float power ){
	float *core;
	int i = 0;
	core = (float *)malloc( size * sizeof(float) );	
	while (1) {		
		core[i]	= power/size;		
		i++;
		if (i==size) break;
	}	
	return core;
}

/* Write double data to txt file
 * parameter:
 * char filename[]: file name
 * double *data: the data to be written
 * int size: the size of the data, usually the count size of an array
 */
void writeData(char filename[], double *data, int size){	
	/* START============Write data to file ========================*/ 
	FILE* f = fopen( filename, "a" );
    if( f )
    {        
        int i;
        char buffer[1000];
        for( i=0; i<size; i++ )
        {
         	sprintf(buffer, "%f,", data[i]);         	
         	fputs(buffer, f);         	
         	//printf("%f\n",data[i]);
        }
        sprintf(buffer, "\n");         	
        fputs(buffer, f);
        fclose(f);
    }
    /*END=============== Write data to file ========================*/  		
}


DustBlock*  newDustBlock()
{
	DustBlock* pack = (DustBlock*)malloc(sizeof(DustBlock));
	pack->row0 = 0;
	pack->col0 = 0;
	pack->row1 = 0;
	pack->col1 = 0;
	pack->childFrag = NULL;
	pack->offsetX = 0;
	pack->offsetY = 0;
	pack->colorOffset = NULL;
	pack->sn = 0;
	pack->flag = 0;	
	return pack;
}

void freeDustBlock(DustBlock* p, int suiside)
{
	if ( p == NULL ) return;
	if ( p->childFrag != NULL )
		freeLinkedFragVector(p->childFrag);
	if ( p->colorOffset != NULL )
		cvReleaseMat( &p->colorOffset );	
	if (suiside)
		free(p);
	return;
}


void freeLinkedFragVector(LinkedFragVector* p)
{
	if (p == NULL) return;

	if( p->next != NULL ){
		freeLinkedFragVector(p->next);
	}
	free(p);	
	return;
}


int dustCheck(DustBlock* pack, TPara* para)
{
	int rt = 0;
	rt = (abs(pack->row1 - pack->row0 + 1) >= para->dustSize.height) 
			&& (abs(pack->col1 - pack->col0 + 1) >= para->dustSize.width);
	return rt;
}

int intMax (int a, int b) {
	return a>b ? a : b;
}

int intMin (int a, int b) {
	return a<b ? a : b;
}


