/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007-2015 Adobe Systems Incorporated                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

/*
	GLator.h
*/

#pragma once

#ifndef GLATOR_H
#define GLATOR_H

typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned short		u_int16;
typedef unsigned long		u_long;
typedef short int			int16;
typedef float				fpshort;

#define PF_TABLE_BITS	12
#define PF_TABLE_SZ_16	4096

#define PF_DEEP_COLOR_AWARE 1	// make sure we get 16bpc pixels; 
								// AE_Effect.h checks for this.
#include "AEConfig.h"

#ifdef AE_OS_WIN
	typedef unsigned short PixelType;
	#include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEFX_ChannelDepthTpl.h"
#include "AEGP_SuiteHandler.h"

#include "GLator_Strings.h"


/* Versioning information */

#define	MAJOR_VERSION	1
#define	MINOR_VERSION	0
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1


/* Parameter defaults */

#define	ALLOT_SLIDER_MIN		0
#define	ALLOT_SLIDER_MAX		5000
#define	ALLOT_SLIDER_DFLT		2500

#define	MULT_SLIDER_MIN		0
#define	MULT_SLIDER_MAX		500
#define	MULT_SLIDER_DFLT		250

#define	ZOOM_SLIDER_MIN		0
#define	ZOOM_SLIDER_MAX		100
#define	ZOOM_SLIDER_DFLT		50

#define	VORON_ZOOM_MIN		0
#define	VORON_MAX		10
#define VORON_ZOOM_DFLT		1

#define RED_DFLT		rand() % 250
#define BLUE_DFLT		rand() % 250
#define GREEN_DFLT		rand() % 250

#define MULT_ANGLE_DFLT		45

enum {
	BEDLM_INPUT = 0,
	BDLM_MODE_AND_COLOR_START,
	BDLM_COLOR,
	BDLM_MODE,
	BDLM_MODE_AND_COLOR_END,
	BDLM_GOLD_NOISE_START,
	BDLM_ALLOTMENT_GOLD,
	BDLM_MULTIPLIER_GOLD,
	BDLM_GOLD_NOISE_END,
	BDLM_GENERIC_NOISE_START,
	BDLM_ALLOTMENT_GENERIC,
	BDLM_MULTIPLIER_GENERIC,
	BDLM_ZOOM_GENERIC,
	BDLM_GENERIC_NOISE_END,
	BDLM_CLASSIC_PERLIN_NOISE_START,
	BDLM_ALLOTMENT_CPERLIN,
	BDLM_MULTIPLIER_CPERLIN,
	BDLM_ZOOM_CPERLIN,
	BDLM_CLASSIC_PERLIN_NOISE_END,
	BDLM_PERLIN_NOISE_START,
	BDLM_ALLOTMENT_PERLIN,
	BDLM_MULTIPLIER_PERLIN,
	BDLM_DIMENSION_PERLIN,
	BDLM_ZOOM_PERLIN,
	BDLM_PERLIN_NOISE_END,
	BDLM_SIMPLEX_NOISE_START,
	BDLM_ALLOTMENT_SIMPLEX,
	BDLM_MULTIPLIER_SIMPLEX,
	BDLM_ZOOM_SIMPLEX,
	BDLM_SIMPLEX_NOISE_END,
	BDLM_CELLVORONOI_START,
	BDLM_ALLOTMENT_CV,
	BDLM_MULTIPLIER_CV,
	BDLM_ZOOM_CV,
	BDLM_CELLVORONOI_END,
	BEDLM_NUM_PARAMS
};

enum {
	BDLM_MODE_DISK_AND_COLOR_START_ID = 1,
	BDLM_DISK_COLOR_ID,
	BDLM_DISK_MODE_ID,
	BDLM_MODE_DISK_AND_COLOR_END_ID,
	BDLM_GOLD_DISK_NOISE_START_ID,
	BDLM_ALLOTMENT_DISK_GOLD_ID,
	BDLM_MULTIPLIER_DISK_GOLD_ID,
	BDLM_GOLD_DISK_NOISE_END_ID,
	BDLM_GENERIC_DISK_NOISE_START_ID,
	BDLM_ALLOTMENT_DISK_GENERIC_ID,
	BDLM_MULTIPLIER_DISK_GENERIC_ID,
	BDLM_ZOOM_DISK_GENERIC_ID,
	BDLM_GENERIC_DISK_NOISE_END_ID,
	BDLM_CLASSIC_DISK_PERLIN_NOISE_START_ID,
	BDLM_ALLOTMENT_DISK_CPERLIN_ID,
	BDLM_MULTIPLIER_DISK_CPERLIN_ID,
	BDLM_ZOOM_DISK_CPERLIN_ID,
	BDLM_CLASSIC_DISK_PERLIN_NOISE_END_ID,
	BDLM_PERLIN_DISK_NOISE_START_ID,
	BDLM_ALLOTMENT_DISK_PERLIN_ID,
	BDLM_MULTIPLIER_DISK_PERLIN_ID,
	BDLM_DIMENSION_DISK_PERLIN_ID,
	BDLM_ZOOM_DISK_PERLIN_ID,
	BDLM_PERLIN_DISK_NOISE_END_ID,
	BDLM_SIMPLEX_DISK_NOISE_START_ID,
	BDLM_ALLOTMENT_DISK_SIMPLEX_ID,
	BDLM_MULTIPLIER_DISK_SIMPLEX_ID,
	BDLM_ZOOM_DISK_SIMPLEX_ID,
	BDLM_SIMPLEX_DISK_NOISE_END_ID,
	BDLM_CELLVORONOI_DISK_START_ID,
	BDLM_ALLOTMENT_DISK_CV_ID,
	BDLM_MULTIPLIER_DISK_CV_ID,
	BDLM_ZOOM_DISK_CV_ID,
	BDLM_CELLVORONOI_DISK_END_ID,
};


extern "C" {
	
	DllExport
	PF_Err 
	EffectMain(
		PF_Cmd			cmd,
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output,
		void			*extra);

}

//helper func
inline u_char AlphaLookup(u_int16 inValSu, u_int16 inMaxSu)
{
	fpshort normValFp = 1.0f - (inValSu)/static_cast<fpshort>(inMaxSu);
	return static_cast<u_char>(normValFp*normValFp*0.8f*255);
}

//error checking macro
#define CHECK(err) {PF_Err err1 = err; if (err1 != PF_Err_NONE ){ throw PF_Err(err1);}}

#endif // GLATOR_H