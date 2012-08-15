/******************************************************************************

 @File         pvrtc_dll.h

 @Title        

 @Version       * Notes        : Extracted from tools.c version 

 @Copyright    Copyright (c) Imagination Technologies Limited. All Rights Reserved. Strictly Confidential.

 @Platform     ANSI

 @Description  header for pngrw.c  (AMTC related file)

******************************************************************************/
#ifndef PVRTCDLL_H
#define PVRTCDLL_H

#ifdef __cplusplus
extern "C" {
#endif

#define DllExport    __declspec( dllexport )

/******************************************************************************/
/*
// Function: 	pvrtc_compress
//
// Description:	Given raw, packed 32 bit ARGB data,
//				this routine generates the compress format as used by PowerVR MBX.
//
// Inputs:		InputARGBData 			Input data as a packed array of bytes in ARGB order (8A 8R 8G 8B)
//
//				OutputCompressedData	Buffer used to store the compressed data. Use pvrtc_size to query
//										the number of bytes needed to store this.
// 								
//				nWidth					Width of the texture. Power of 2 sizes currently supported, 
//										from 8(16 for 2bpp) through to 2048. For sizes under 8(16),
//										simply repeat the data until it fills 8(16) texels.
//
//				nHeight					Height of the texture. Power of 2 sizes currently supported, 
//										from 8 through to 2048. For sizes under 8,
//										simply repeat the data until it fills 8 texels.
//
//				bMipMap					Generate all mipmap levels. Recommended for rendering speed & 
//										quality
//
//				bAlphaOn				Is alpha data supplied? If not, assumes texture is opaque.
//
//              bUse2bitFormat			1=2bit per pixel format. 0=4bit per pixel.
//
//				iQuality				Value from 0 to 4, the higher the number, the higher the quality.
*/
/******************************************************************************/
DllExport int pvrtc_compress(	void*	InputARGBData,
					void*	OutputCompressedData,
					int		nWidth,			
					int		nHeight, 
					int		bMipMap,	    
					int		bAlphaOn,
					int     bUse2bitFormat,
					int		iQuality);

/******************************************************************************/
/*
// Function: 	pvrtc_decompress
//
// Description: Given an array of pixels compressed using PVR texture compression
//              generates the row 32-bit ARGB (8A8R8G8B) array.
//
// Inputs:		InputCompressedData		Supplied data of stored compressed pixels.
//
//				OutputARGBData			Output data as a packed array of bytes (8A 8R 8G 8B)
//
//				nWidth					Width of the MIP map level you wish to decompress. Power of 2 
//										sizes currently supported, from 8(16 for 2bpp) through to 2048. 
//										For sizes under 8(16), data is simply repeated until it fills 
//										8(16) pixels. For decompression, simply discard unneeded pixels.
//
//				nHeight					Height of the MIP map level you wish to decompress. Power of 2 
//										sizes currently supported, from 8 through to 2048. For sizes
//										under 8, data is simply repeated until it fills 8 pixels. For
//										decompression, simply discard unneeded pixels.
//
//				nMipmapLevel			The mip map level to decompress. Lowest mip-levels are 
//										minimally sized to 8(16) by 8, see nWidth/nHeight for details.
//
//              bUse2bitFormat			1=2bit per pixel format. 0=4bit per pixel.
*/
/******************************************************************************/
DllExport int pvrtc_decompress(	void*	OutputARGBData,
						void*	InputCompressedData,
						int		nWidth,			
						int		nHeight, 		
						int     nMipmapLevel,
						int     bUse2bitFormat);

/*************************************************************************
// Use it for querying the final size after compression, useful to call before compression.
//				nWidth			Current texture width
//				nHeight			Current texture height
//				bMipMap			true = include size of all mip maps that will be generated.
//				bUse2bitFormat  2bit per pixel format. Otherwise 4bit per pixel.
**************************************************************************/

DllExport int pvrtc_size(	int		nWidth,				
				int		nHeight, 		
				int		bMipMap,		
				int     bUse2bitFormat);

/******************************************************************************/
/*
// Function: 	pvrtc_get_version
//
// Description: Returns the version of PVRTC.
//
*/
/******************************************************************************/

DllExport void pvrtc_get_version(unsigned int *uMajor, unsigned int *uMinor,
								unsigned int *uInternal, unsigned int *uBuild);

/************************************************************************
// pvrtc_info_output. 
// default output = NULL (no output)
// Other values are stdout, stderr or a file created by the user
*************************************************************************/
DllExport void pvrtc_info_output(FILE *DebugOutput);

#ifdef __cplusplus
}
#endif

#endif /* PVRTCDLL_H */
/*
// END OF FILE
*/ 

