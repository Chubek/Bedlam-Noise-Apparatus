/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
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

#include "GLator.h"


typedef struct {
	unsigned long	index;
	char			str[256];
} TableString;


TableString		g_strs[StrID_NUMTYPES] = {
	StrID_NONE,						"",
	StrID_Name,						"Bedlam Noise Apparatus",
	StrID_Description,				"A noise generator written by Chubak Bidpaa.\n Copyright 2019. \n Thanks to Stefan Gustavson, Inigo Quilez, Pietro De Nicola and Ian McEwan.",
	StrID_Color_Param_Name,			"Color",
	StrID_Mode_And_Color,			"Color and Mode",
	StrID_Color,					"Noise Color",
	StrID_Mode,						"Noise Mode",
	StrID_Opacity,					"Opacity",
	StrID_Blend,					"Blend Mode",
	StrID_Gold,						"Gold Noise",
	StrID_Generic,					"Generic Noise",
	StrID_Perlic_Classic,			"Classic Perlin",
	StrID_Perlin,					"Perlin Noise",
	StrID_Simplex,					"Simplex Noise",
	StrID_Voron,					"Voronoi Noise",
	StrID_Allot,					"Seed",	
	StrID_Mult,						"Multiplier",
	StrID_Zoom,						"Zoom",
	StrID_Dim,						"Dimension",
	StrID_Dim_Mult,					"Dimension Multiplier",
	StrID_Checkbox_Param_Name,		"Use Downsample Factors",
	StrID_Checkbox_Description,		"Correct at all resolutions",
	StrID_DependString1,			"All Dependencies requested.",
	StrID_DependString2,			"Missing Dependencies requested.",
	StrID_Err_LoadSuite,			"Error loading suite.",
	StrID_Err_FreeSuite,			"Error releasing suite.",
	StrID_3D_Param_Name,			"Use lights and cameras",
	StrID_3D_Param_Description,		""
};


char	*GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}

	