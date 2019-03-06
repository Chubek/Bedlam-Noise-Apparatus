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

/*	GLator.cpp	

	This is a sample OpenGL plugin. The framework is done for you.
	Use it to create more funky effects.
	
	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			Win and Mac versions use the same base files.			anindyar	7/4/2007
	1.1			Add OpenGL context switching to play nicely with
				AE's own OpenGL usage (thanks Brendan Bolles!)			zal			8/13/2012
	2.0			Completely re-written for OGL 3.3 and threads			aparente	9/30/2015
	2.1			Added new entry point									zal			9/15/2017

*/

#include "GLator.h"

#include "GL_base.h"
#include "Smart_Utils.h"
#include "AEFX_SuiteHelper.h"

#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include "vmath.hpp"
#include <assert.h>

using namespace AESDK_OpenGL;
using namespace gl33core;

#include "glbinding/gl33ext/gl.h"
#include <glbinding/gl/extension.h>

/* AESDK_OpenGL effect specific variables */

namespace {

	struct Noise
	{
		float R;
		float G;
		float B;
		float allot;
		float mult;
		float dim;
		float zoom;
		float mode;

		Noise() = default;

		Noise(float _R, float _G, float _B, float _allot, float _mult, float _dim, float _zoom)
		{
			R = _R;
			G = _G;
			B = _B;
			allot = _allot;
			mult = _mult;
			dim = _dim;
			zoom = _zoom;
		}

	};

	


	THREAD_LOCAL int t_thread = -1;

	std::atomic_int S_cnt;
	std::map<int, std::shared_ptr<AESDK_OpenGL::AESDK_OpenGL_EffectRenderData> > S_render_contexts;
	std::recursive_mutex S_mutex;

	AESDK_OpenGL::AESDK_OpenGL_EffectCommonDataPtr S_GLator_EffectCommonData; //global context
	std::string S_ResourcePath;

	// - OpenGL resources are restricted per thread, mimicking the OGL driver
	// - The filter will eliminate all TLS (Thread Local Storage) at PF_Cmd_GLOBAL_SETDOWN
	AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr GetCurrentRenderContext()
	{
		S_mutex.lock();
		AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr result;

		if (t_thread == -1) {
			t_thread = S_cnt++;

			result.reset(new AESDK_OpenGL::AESDK_OpenGL_EffectRenderData());
			S_render_contexts[t_thread] = result;
		}
		else {
			result = S_render_contexts[t_thread];
		}
		S_mutex.unlock();
		return result;
	}

#ifdef AE_OS_WIN
	std::string get_string_from_wcs(const wchar_t* pcs)
	{
		int res = WideCharToMultiByte(CP_ACP, 0, pcs, -1, NULL, 0, NULL, NULL);

		std::auto_ptr<char> shared_pbuf(new char[res]);

		char *pbuf = shared_pbuf.get();

		res = WideCharToMultiByte(CP_ACP, 0, pcs, -1, pbuf, res, NULL, NULL);

		return std::string(pbuf);
	}
#endif

	void RenderQuad(GLuint vbo)
	{
		glEnableVertexAttribArray(PositionSlot);
		glEnableVertexAttribArray(UVSlot);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(PositionSlot, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
		glVertexAttribPointer(UVSlot, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisableVertexAttribArray(PositionSlot);
		glDisableVertexAttribArray(UVSlot);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	std::string GetResourcesPath(PF_InData		*in_data)
	{
		//initialize and compile the shader objects
		A_UTF16Char pluginFolderPath[AEFX_MAX_PATH];
		PF_GET_PLATFORM_DATA(PF_PlatData_EXE_FILE_PATH_W, &pluginFolderPath);

#ifdef AE_OS_WIN
		std::string resourcePath = get_string_from_wcs((wchar_t*)pluginFolderPath);
		std::string::size_type pos;
		//delete the plugin name
		pos = resourcePath.rfind("\\", resourcePath.length());
		resourcePath = resourcePath.substr(0, pos) + "\\";
#endif
#ifdef AE_OS_MAC
		NSUInteger length = 0;
		A_UTF16Char* tmp = pluginFolderPath;
		while (*tmp++ != 0) {
			++length;
		}
		NSString* newStr = [[NSString alloc] initWithCharacters:pluginFolderPath length : length];
		std::string resourcePath([newStr UTF8String]);
		resourcePath += "/Contents/Resources/";
#endif
		return resourcePath;
	}

	struct CopyPixelFloat_t {
		PF_PixelFloat	*floatBufferP;
		PF_EffectWorld	*input_worldP;
	};

	PF_Err
	CopyPixelFloatIn(
		void			*refcon,
		A_long			x,
		A_long			y,
		PF_PixelFloat	*inP,
		PF_PixelFloat	*)
	{
		CopyPixelFloat_t	*thiS = reinterpret_cast<CopyPixelFloat_t*>(refcon);
		PF_PixelFloat		*outP = thiS->floatBufferP + y * thiS->input_worldP->width + x;

		outP->red = inP->red;
		outP->green = inP->green;
		outP->blue = inP->blue;
		outP->alpha = inP->alpha;

		return PF_Err_NONE;
	}

	PF_Err
	CopyPixelFloatOut(
		void			*refcon,
		A_long			x,
		A_long			y,
		PF_PixelFloat	*,
		PF_PixelFloat	*outP)
	{
		CopyPixelFloat_t		*thiS = reinterpret_cast<CopyPixelFloat_t*>(refcon);
		const PF_PixelFloat		*inP = thiS->floatBufferP + y * thiS->input_worldP->width + x;

		outP->red = inP->red;
		outP->green = inP->green;
		outP->blue = inP->blue;
		outP->alpha = inP->alpha;

		return PF_Err_NONE;
	}


	gl::GLuint UploadTexture(AEGP_SuiteHandler& suites,					// >>
							 PF_PixelFormat			format,				// >>
							 PF_EffectWorld			*input_worldP,		// >>
							 PF_EffectWorld			*output_worldP,		// >>
							 PF_InData				*in_data,			// >>
							 size_t& pixSizeOut,						// <<
							 gl::GLenum& glFmtOut,						// <<
							 float& multiplier16bitOut)					// <<
	{
		// - upload to texture memory
		// - we will convert on-the-fly from ARGB to RGBA, and also to pre-multiplied alpha,
		// using a fragment shader
#ifdef _DEBUG
		GLint nUnpackAlignment;
		::glGetIntegerv(GL_UNPACK_ALIGNMENT, &nUnpackAlignment);
		assert(nUnpackAlignment == 4);
#endif

		gl::GLuint inputFrameTexture;
		glGenTextures(1, &inputFrameTexture);
		glBindTexture(GL_TEXTURE_2D, inputFrameTexture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA32F, input_worldP->width, input_worldP->height, 0, GL_RGBA, GL_FLOAT, nullptr);

		multiplier16bitOut = 1.0f;
		switch (format)
		{
		case PF_PixelFormat_ARGB128:
		{
			glFmtOut = GL_FLOAT;
			pixSizeOut = sizeof(PF_PixelFloat);

			std::auto_ptr<PF_PixelFloat> bufferFloat(new PF_PixelFloat[input_worldP->width * input_worldP->height]);
			CopyPixelFloat_t refcon = { bufferFloat.get(), input_worldP };

			CHECK(suites.IterateFloatSuite1()->iterate(in_data,
				0,
				input_worldP->height,
				input_worldP,
				nullptr,
				reinterpret_cast<void*>(&refcon),
				CopyPixelFloatIn,
				output_worldP));

			glPixelStorei(GL_UNPACK_ROW_LENGTH, input_worldP->width);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, input_worldP->width, input_worldP->height, GL_RGBA, GL_FLOAT, bufferFloat.get());
			break;
		}

		case PF_PixelFormat_ARGB64:
		{
			glFmtOut = GL_UNSIGNED_SHORT;
			pixSizeOut = sizeof(PF_Pixel16);
			multiplier16bitOut = 65535.0f / 32768.0f;

			glPixelStorei(GL_UNPACK_ROW_LENGTH, input_worldP->rowbytes / sizeof(PF_Pixel16));
			PF_Pixel16 *pixelDataStart = NULL;
			PF_GET_PIXEL_DATA16(input_worldP, NULL, &pixelDataStart);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, input_worldP->width, input_worldP->height, GL_RGBA, GL_UNSIGNED_SHORT, pixelDataStart);
			break;
		}

		case PF_PixelFormat_ARGB32:
		{
			glFmtOut = GL_UNSIGNED_BYTE;
			pixSizeOut = sizeof(PF_Pixel8);

			glPixelStorei(GL_UNPACK_ROW_LENGTH, input_worldP->rowbytes / sizeof(PF_Pixel8));
			PF_Pixel8 *pixelDataStart = NULL;
			PF_GET_PIXEL_DATA8(input_worldP, NULL, &pixelDataStart);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, input_worldP->width, input_worldP->height, GL_RGBA, GL_UNSIGNED_BYTE, pixelDataStart);
			break;
		}

		default:
			CHECK(PF_Err_BAD_CALLBACK_PARAM);
			break;
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		//unbind all textures
		glBindTexture(GL_TEXTURE_2D, 0);

		return inputFrameTexture;
	}

	void ReportIfErrorFramebuffer(PF_InData *in_data, PF_OutData *out_data)
	{
		// Check for errors...
		std::string error_msg;
		if ((error_msg = CheckFramebufferStatus()) != std::string("OK"))
		{
			out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
			PF_SPRINTF(out_data->return_msg, error_msg.c_str());
			CHECK(PF_Err_OUT_OF_MEMORY);
		}
	}


	void SwizzleGL(const AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr& renderContext,
				   A_long widthL, A_long heightL,
				   gl::GLuint		inputFrameTexture,
					std::map<std::string, Noise>	noises,
				   float			multiplier16bit)
	{
		glBindTexture(GL_TEXTURE_2D, inputFrameTexture);

		glUseProgram(renderContext->mProgramObj2Su);

		// view matrix, mimic windows coordinates
		vmath::Matrix4 ModelviewProjection = vmath::Matrix4::translation(vmath::Vector3(-1.0f, -1.0f, 0.0f)) *
			vmath::Matrix4::scale(vmath::Vector3(2.0 / float(widthL), 2.0 / float(heightL), 1.0f));

		GLint location = glGetUniformLocation(renderContext->mProgramObj2Su, "ModelviewProjection");
		glUniformMatrix4fv(location, 1, GL_FALSE, (GLfloat*)&ModelviewProjection);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "multiplier16bit");
		glUniform1f(location, multiplier16bit);

		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Gold.R");
		glUniform1f(location, noises["Gold Noise"].R);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Generic.R");
		glUniform1f(location, noises["Generic Noise"].R);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "CPerlin.R");
		glUniform1f(location, noises["Classic Perlin"].R);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Perlin.R");
		glUniform1f(location, noises["Perlin Noise"].R);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Simplex.R");
		glUniform1f(location, noises["Simplex Noise"].R);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Voronoi.R");
		glUniform1f(location, noises["Voronoi Noise"].R);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Gold.G");
		glUniform1f(location, noises["Gold Noise"].G);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Generic.G");
		glUniform1f(location, noises["Generic Noise"].G);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "CPerlin.G");
		glUniform1f(location, noises["Classic Perlin"].G);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Perlin.G");
		glUniform1f(location, noises["Perlin Noise"].G);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Simplex.G");
		glUniform1f(location, noises["Simplex Noise"].G);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Voronoi.G");
		glUniform1f(location, noises["Voronoi Noise"].G);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Gold.B");
		glUniform1f(location, noises["Gold Noise"].B);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Generic.B");
		glUniform1f(location, noises["Generic Noise"].B);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "CPerlin.B");
		glUniform1f(location, noises["Classic Perlin"].B);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Perlin.B");
		glUniform1f(location, noises["Perlin Noise"].B);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Simplex.B");
		glUniform1f(location, noises["Simplex Noise"].B);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Voronoi.B");
		glUniform1f(location, noises["Voronoi Noise"].B);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Gold.allot");
		glUniform1f(location, noises["Gold Noise"].allot);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Generic.allot");
		glUniform1f(location, noises["Generic Noise"].allot);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "CPerlin.allot");
		glUniform1f(location, noises["Classic Perlin"].allot);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Perlin.allot");
		glUniform1f(location, noises["Perlin Noise"].allot);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Simplex.allot");
		glUniform1f(location, noises["Simplex Noise"].allot);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Voronoi.allot");
		glUniform1f(location, noises["Voronoi Noise"].allot);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Gold.mult");
		glUniform1f(location, noises["Gold Noise"].mult);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Generic.mult");
		glUniform1f(location, noises["Generic Noise"].mult);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "CPerlin.mult");
		glUniform1f(location, noises["Classic Perlin"].mult);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Perlin.mult");
		glUniform1f(location, noises["Perlin Noise"].mult);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Simplex.mult");
		glUniform1f(location, noises["Simplex Noise"].mult);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Voronoi.mult");
		glUniform1f(location, noises["Voronoi Noise"].mult);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Gold.dim");
		glUniform1f(location, noises["Gold Noise"].dim);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Generic.dim");
		glUniform1f(location, noises["Generic Noise"].dim);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "CPerlin.dim");
		glUniform1f(location, noises["Classic Perlin"].dim);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Perlin.dim");
		glUniform1f(location, noises["Perlin Noise"].dim);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Simplex.dim");
		glUniform1f(location, noises["Simplex Noise"].dim);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Voronoi.dim");
		glUniform1f(location, noises["Voronoi Noise"].dim);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Gold.zoom");
		glUniform1f(location, noises["Gold Noise"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Generic.zoom");
		glUniform1f(location, noises["Generic Noise"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "CPerlin.zoom");
		glUniform1f(location, noises["Classic Perlin"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Perlin.zoom");
		glUniform1f(location, noises["Perlin Noise"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Simplex.zoom");
		glUniform1f(location, noises["Simplex Noise"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Voronoi.zoom");
		glUniform1f(location, noises["Voronoi Noise"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Gold.mode");
		glUniform1f(location, noises["Gold Noise"].mode);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Generic.mode");
		glUniform1f(location, noises["Generic Noise"].mode);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "CPerlin.mode");
		glUniform1f(location, noises["Classic Perlin"].mode);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Perlin.mode");
		glUniform1f(location, noises["Perlin Noise"].mode);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Simplex.mode");
		glUniform1f(location, noises["Simplex Noise"].mode);
		location = glGetUniformLocation(renderContext->mProgramObj2Su, "Voronoi.mode");
		glUniform1f(location, noises["Voronoi Noise"].mode);


		AESDK_OpenGL_BindTextureToTarget(renderContext->mProgramObj2Su, inputFrameTexture, std::string("videoTexture"));

		// render
		glBindVertexArray(renderContext->vao);
		RenderQuad(renderContext->quad);
		glBindVertexArray(0);

		glUseProgram(0);

		glFlush();
	}

	void RenderGL(const AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr& renderContext,
				  A_long widthL, A_long heightL,
				  gl::GLuint		inputFrameTexture,
				  std::map<std::string, Noise>			noises,
				  float				multiplier16bit)
	{
		// - make sure we blend correctly inside the framebuffer
		// - even though we just cleared it, another effect may want to first
		// draw some kind of background to blend with
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquation(GL_FUNC_ADD);

		// view matrix, mimic windows coordinates
		vmath::Matrix4 ModelviewProjection = vmath::Matrix4::translation(vmath::Vector3(-1.0f, -1.0f, 0.0f)) *
			vmath::Matrix4::scale(vmath::Vector3(2.0 / float(widthL), 2.0 / float(heightL), 1.0f));

		glBindTexture(GL_TEXTURE_2D, inputFrameTexture);

		glUseProgram(renderContext->mProgramObjSu);

		// program uniforms
		GLint location = glGetUniformLocation(renderContext->mProgramObjSu, "ModelviewProjection");
		glUniformMatrix4fv(location, 1, GL_FALSE, (GLfloat*)&ModelviewProjection);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Gold.R");
		glUniform1f(location, noises["Gold Noise"].R);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Generic.R");
		glUniform1f(location, noises["Generic Noise"].R);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "CPerlin.R");
		glUniform1f(location, noises["Classic Perlin"].R);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Perlin.R");
		glUniform1f(location, noises["Perlin Noise"].R);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Simplex.R");
		glUniform1f(location, noises["Simplex Noise"].R);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Voronoi.R");
		glUniform1f(location, noises["Voronoi Noise"].R);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Gold.G");
		glUniform1f(location, noises["Gold Noise"].G);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Generic.G");
		glUniform1f(location, noises["Generic Noise"].G);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "CPerlin.G");
		glUniform1f(location, noises["Classic Perlin"].G);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Perlin.G");
		glUniform1f(location, noises["Perlin Noise"].G);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Simplex.G");
		glUniform1f(location, noises["Simplex Noise"].G);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Voronoi.G");
		glUniform1f(location, noises["Voronoi Noise"].G);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Gold.B");
		glUniform1f(location, noises["Gold Noise"].B);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Generic.B");
		glUniform1f(location, noises["Generic Noise"].B);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "CPerlin.B");
		glUniform1f(location, noises["Classic Perlin"].B);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Perlin.B");
		glUniform1f(location, noises["Perlin Noise"].B);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Simplex.B");
		glUniform1f(location, noises["Simplex Noise"].B);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Voronoi.B");
		glUniform1f(location, noises["Voronoi Noise"].B);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Gold.allot");
		glUniform1f(location, noises["Gold Noise"].allot);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Generic.allot");
		glUniform1f(location, noises["Generic Noise"].allot);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "CPerlin.allot");
		glUniform1f(location, noises["Classic Perlin"].allot);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Perlin.allot");
		glUniform1f(location, noises["Perlin Noise"].allot);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Simplex.allot");
		glUniform1f(location, noises["Simplex Noise"].allot);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Voronoi.allot");
		glUniform1f(location, noises["Voronoi Noise"].allot);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Gold.mult");
		glUniform1f(location, noises["Gold Noise"].mult);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Generic.mult");
		glUniform1f(location, noises["Generic Noise"].mult);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "CPerlin.mult");
		glUniform1f(location, noises["Classic Perlin"].mult);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Perlin.mult");
		glUniform1f(location, noises["Perlin Noise"].mult);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Simplex.mult");
		glUniform1f(location, noises["Simplex Noise"].mult);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Voronoi.mult");
		glUniform1f(location, noises["Voronoi Noise"].mult);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Gold.dim");
		glUniform1f(location, noises["Gold Noise"].dim);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Generic.dim");
		glUniform1f(location, noises["Generic Noise"].dim);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "CPerlin.dim");
		glUniform1f(location, noises["Classic Perlin"].dim);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Perlin.dim");
		glUniform1f(location, noises["Perlin Noise"].dim);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Simplex.dim");
		glUniform1f(location, noises["Simplex Noise"].dim);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Voronoi.dim");
		glUniform1f(location, noises["Voronoi Noise"].dim);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Gold.zoom");
		glUniform1f(location, noises["Gold Noise"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Generic.zoom");
		glUniform1f(location, noises["Generic Noise"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "CPerlin.zoom");
		glUniform1f(location, noises["Classic Perlin"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Perlin.zoom");
		glUniform1f(location, noises["Perlin Noise"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Simplex.zoom");
		glUniform1f(location, noises["Simplex Noise"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Voronoi.zoom");
		glUniform1f(location, noises["Voronoi Noise"].zoom);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Gold.mode");
		glUniform1f(location, noises["Gold Noise"].mode);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Generic.mode");
		glUniform1f(location, noises["Generic Noise"].mode);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "CPerlin.mode");
		glUniform1f(location, noises["Classic Perlin"].mode);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Perlin.mode");
		glUniform1f(location, noises["Perlin Noise"].mode);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Simplex.mode");
		glUniform1f(location, noises["Simplex Noise"].mode);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "Voronoi.mode");
		glUniform1f(location, noises["Voronoi Noise"].mode);
		location = glGetUniformLocation(renderContext->mProgramObjSu, "multiplier16bit");
		glUniform1f(location, multiplier16bit);

		// Identify the texture to use and bind it to texture unit 0
		AESDK_OpenGL_BindTextureToTarget(renderContext->mProgramObjSu, inputFrameTexture, std::string("videoTexture"));

		// render
		glBindVertexArray(renderContext->vao);
		RenderQuad(renderContext->quad);
		glBindVertexArray(0);

		glUseProgram(0);
		glDisable(GL_BLEND);
	}

	void DownloadTexture(const AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr& renderContext,
						 AEGP_SuiteHandler&		suites,				// >>
						 PF_EffectWorld			*input_worldP,		// >>
						 PF_EffectWorld			*output_worldP,		// >>
						 PF_InData				*in_data,			// >>
						 PF_PixelFormat			format,				// >>
						 size_t					pixSize,			// >>
						 gl::GLenum				glFmt				// >>
						 )
	{
		//download from texture memory onto the same surface
		PF_Handle bufferH = NULL;
		bufferH = suites.HandleSuite1()->host_new_handle(((renderContext->mRenderBufferWidthSu * renderContext->mRenderBufferHeightSu)* pixSize));
		if (!bufferH) {
			CHECK(PF_Err_OUT_OF_MEMORY);
		}
		void *bufferP = suites.HandleSuite1()->host_lock_handle(bufferH);

		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, renderContext->mRenderBufferWidthSu, renderContext->mRenderBufferHeightSu, GL_RGBA, glFmt, bufferP);

		switch (format)
		{
		case PF_PixelFormat_ARGB128:
		{
			PF_PixelFloat* bufferFloatP = reinterpret_cast<PF_PixelFloat*>(bufferP);
			CopyPixelFloat_t refcon = { bufferFloatP, input_worldP };

			CHECK(suites.IterateFloatSuite1()->iterate(in_data,
				0,
				input_worldP->height,
				input_worldP,
				nullptr,
				reinterpret_cast<void*>(&refcon),
				CopyPixelFloatOut,
				output_worldP));
			break;
		}

		case PF_PixelFormat_ARGB64:
		{
			PF_Pixel16* buffer16P = reinterpret_cast<PF_Pixel16*>(bufferP);

			//copy to output_worldP
			for (int y = 0; y < output_worldP->height; ++y)
			{
				PF_Pixel16 *pixelDataStart = NULL;
				PF_GET_PIXEL_DATA16(output_worldP, NULL, &pixelDataStart);
				::memcpy(pixelDataStart + (y * output_worldP->rowbytes / sizeof(PF_Pixel16)),
					buffer16P + (y * renderContext->mRenderBufferWidthSu),
					output_worldP->width * sizeof(PF_Pixel16));
			}
			break;
		}

		case PF_PixelFormat_ARGB32:
		{
			PF_Pixel8 *buffer8P = reinterpret_cast<PF_Pixel8*>(bufferP);

			//copy to output_worldP
			for (int y = 0; y < output_worldP->height; ++y)
			{
				PF_Pixel8 *pixelDataStart = NULL;
				PF_GET_PIXEL_DATA8(output_worldP, NULL, &pixelDataStart);
				::memcpy(pixelDataStart + (y * output_worldP->rowbytes / sizeof(PF_Pixel8)),
					buffer8P + (y * renderContext->mRenderBufferWidthSu),
					output_worldP->width * sizeof(PF_Pixel8));
			}
			break;
		}

		default:
			CHECK(PF_Err_BAD_CALLBACK_PARAM);
			break;
		}

		//clean the data after being copied
		suites.HandleSuite1()->host_unlock_handle(bufferH);
		suites.HandleSuite1()->host_dispose_handle(bufferH);
	}
} // anonymous namespace

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
											"%s v%d.%d\r%s",
											STR(StrID_Name), 
											MAJOR_VERSION, 
											MINOR_VERSION, 
											STR(StrID_Description));
	return PF_Err_NONE;
}

static PF_Err
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	out_data->out_flags = 	PF_OutFlag_DEEP_COLOR_AWARE;
	
	out_data->out_flags2 =	PF_OutFlag2_FLOAT_COLOR_AWARE	|
							PF_OutFlag2_SUPPORTS_SMART_RENDER;
	
	PF_Err err = PF_Err_NONE;
	try
	{
		// always restore back AE's own OGL context
		SaveRestoreOGLContext oSavedContext;
		AEGP_SuiteHandler suites(in_data->pica_basicP);

		//Now comes the OpenGL part - OS specific loading to start with
		S_GLator_EffectCommonData.reset(new AESDK_OpenGL::AESDK_OpenGL_EffectCommonData());
		AESDK_OpenGL_Startup(*S_GLator_EffectCommonData.get());
		
		S_ResourcePath = GetResourcesPath(in_data);
	}
	catch(PF_Err& thrown_err)
	{
		err = thrown_err;
	}
	catch (...)
	{
		err = PF_Err_OUT_OF_MEMORY;
	}

	return err;
}

static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	AEFX_CLR_STRUCT(def);

	PF_ADD_TOPIC(STR(StrID_Mode_And_Color),
		BDLM_MODE_DISK_AND_COLOR_START_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_COLOR(STR(StrID_Color),
		RED_DFLT,
		GREEN_DFLT,
		BLUE_DFLT,
		BDLM_DISK_COLOR_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP(STR(StrID_Mode),
		6,
		1,
		("Gold | Generic | Perlin CLassic | Perlin | Simplex | Cell/Voronoi"),
		BDLM_DISK_MODE_ID);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(BDLM_MODE_DISK_AND_COLOR_END_ID);
	AEFX_CLR_STRUCT(def);

	PF_ADD_TOPIC(STR(StrID_Gold),
		BDLM_GOLD_DISK_NOISE_START_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Allot),
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_ALLOTMENT_DISK_GOLD_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_ANGLE(STR(StrID_Mult),
		MULT_ANGLE_DFLT,
		BDLM_MULTIPLIER_DISK_GOLD_ID);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(BDLM_GOLD_DISK_NOISE_END_ID);
	AEFX_CLR_STRUCT(def);


	PF_ADD_TOPIC(STR(StrID_Generic),
		BDLM_GENERIC_DISK_NOISE_START_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Allot),
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_ALLOTMENT_DISK_GENERIC_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_ANGLE(STR(StrID_Mult),
		MULT_ANGLE_DFLT,
		BDLM_MULTIPLIER_DISK_GENERIC_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Zoom),
		ZOOM_SLIDER_MIN,
		ZOOM_SLIDER_MAX,
		ZOOM_SLIDER_MIN,
		ZOOM_SLIDER_MAX,
		ZOOM_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_ZOOM_DISK_GENERIC_ID);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(BDLM_GENERIC_DISK_NOISE_END_ID);
	AEFX_CLR_STRUCT(def);


	PF_ADD_TOPIC(STR(StrID_Perlic_Classic),
		BDLM_CLASSIC_DISK_PERLIN_NOISE_START_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Allot),
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_ALLOTMENT_DISK_CPERLIN_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_ANGLE(STR(StrID_Mult),
		MULT_ANGLE_DFLT,
		BDLM_MULTIPLIER_DISK_CPERLIN_ID);		
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Zoom),
		ZOOM_SLIDER_MIN,
		ZOOM_SLIDER_MAX,
		ZOOM_SLIDER_MIN,
		ZOOM_SLIDER_MAX,
		ZOOM_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_ZOOM_DISK_CPERLIN_ID);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(BDLM_CLASSIC_DISK_PERLIN_NOISE_END_ID);
	AEFX_CLR_STRUCT(def);


	PF_ADD_TOPIC(STR(StrID_Perlin),
		BDLM_PERLIN_DISK_NOISE_START_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Allot),
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_ALLOTMENT_DISK_PERLIN_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_ANGLE(STR(StrID_Mult),
		MULT_ANGLE_DFLT,
		BDLM_MULTIPLIER_DISK_PERLIN_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Dim),
		MULT_SLIDER_MIN,
		MULT_SLIDER_MAX,
		MULT_SLIDER_MIN,
		MULT_SLIDER_MAX,
		MULT_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_DIMENSION_DISK_PERLIN_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Zoom),
		ZOOM_SLIDER_MIN,
		ZOOM_SLIDER_MAX,
		ZOOM_SLIDER_MIN,
		ZOOM_SLIDER_MAX,
		ZOOM_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_ZOOM_DISK_PERLIN_ID);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(BDLM_PERLIN_DISK_NOISE_END_ID);
	AEFX_CLR_STRUCT(def);


	PF_ADD_TOPIC(STR(StrID_Simplex),
		BDLM_SIMPLEX_DISK_NOISE_START_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Allot),
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_ALLOTMENT_DISK_SIMPLEX_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_ANGLE(STR(StrID_Mult),
		MULT_ANGLE_DFLT,
		BDLM_MULTIPLIER_DISK_SIMPLEX_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Zoom),
		ZOOM_SLIDER_MIN,
		ZOOM_SLIDER_MAX,
		ZOOM_SLIDER_MIN,
		ZOOM_SLIDER_MAX,
		ZOOM_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_ZOOM_DISK_SIMPLEX_ID);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(BDLM_SIMPLEX_DISK_NOISE_END_ID);
	AEFX_CLR_STRUCT(def);



	PF_ADD_TOPIC(STR(StrID_Voron),
		BDLM_CELLVORONOI_DISK_START_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Allot),
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_MIN,
		ALLOT_SLIDER_MAX,
		ALLOT_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_ALLOTMENT_DISK_CV_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_ANGLE(STR(StrID_Mult),
		MULT_ANGLE_DFLT,
		BDLM_MULTIPLIER_DISK_CV_ID);
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Zoom),
		ZOOM_SLIDER_MIN,
		ZOOM_SLIDER_MAX,
		ZOOM_SLIDER_MIN,
		ZOOM_SLIDER_MAX,
		ZOOM_SLIDER_DFLT,
		PF_Precision_THOUSANDTHS,
		0,
		0,
		BDLM_ZOOM_DISK_CV_ID);
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(BDLM_CELLVORONOI_DISK_END_ID);
	AEFX_CLR_STRUCT(def);



	out_data->num_params = BEDLM_NUM_PARAMS;

	return err;
}


static PF_Err 
GlobalSetdown (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err			err			=	PF_Err_NONE;

	try
	{
		// always restore back AE's own OGL context
		SaveRestoreOGLContext oSavedContext;

		S_mutex.lock();
		S_render_contexts.clear();
		S_mutex.unlock();

		//OS specific unloading
		AESDK_OpenGL_Shutdown(*S_GLator_EffectCommonData.get());
		S_GLator_EffectCommonData.reset();
		S_ResourcePath.clear();

		if (in_data->sequence_data) {
			PF_DISPOSE_HANDLE(in_data->sequence_data);
			out_data->sequence_data = NULL;
		}
	}
	catch(PF_Err& thrown_err)
	{
		err = thrown_err;
	}
	catch (...)
	{
		err = PF_Err_OUT_OF_MEMORY;
	}

	return err;
}

static PF_Err
PreRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_PreRenderExtra		*extra)
{
	PF_Err	err = PF_Err_NONE,
			err2 = PF_Err_NONE;

	PF_ParamDef		ModeAndColorStart;
	PF_ParamDef		Color;
	PF_ParamDef		Mode;
	PF_ParamDef		ModeAndColorEnd;
	PF_ParamDef		GoldNoiseStart;
	PF_ParamDef		AllotmentGold;
	PF_ParamDef		MultiplierGold;
	PF_ParamDef		GoldNoiseEnd;
	PF_ParamDef		GenericNoiseStart;
	PF_ParamDef		AllotmentGeneric;
	PF_ParamDef		MultiplierGeneric;
	PF_ParamDef		ZoomGeneric;
	PF_ParamDef		GenericNoiseEnd;
	PF_ParamDef		ClassicPerlinNoiseStart;
	PF_ParamDef		AllotmentCperlin;
	PF_ParamDef		MultiplierCperlin;
	PF_ParamDef		ZoomCperlin;
	PF_ParamDef		ClassicPerlinNoiseEnd;
	PF_ParamDef		PerlinNoiseStart;
	PF_ParamDef		AllotmentPerlin;
	PF_ParamDef		MultiplierPerlin;
	PF_ParamDef		DimensionPerlin;
	PF_ParamDef		ZoomPerlin;
	PF_ParamDef		PerlinNoiseEnd;
	PF_ParamDef		SimplexNoiseStart;
	PF_ParamDef		AllotmentSimplex;
	PF_ParamDef		MultiplierSimplex;
	PF_ParamDef		ZoomSimplex;
	PF_ParamDef		SimplexNoiseEnd;
	PF_ParamDef		CellvoronoiStart;
	PF_ParamDef		AllotmentCv;
	PF_ParamDef		MultiplierCv;
	PF_ParamDef		ZoomCv;
	PF_ParamDef		CellvoronoiEnd;

	PF_RenderRequest req = extra->input->output_request;
	PF_CheckoutResult in_result;

	AEFX_CLR_STRUCT(ModeAndColorStart);
	AEFX_CLR_STRUCT(Color);
	AEFX_CLR_STRUCT(Mode);
	AEFX_CLR_STRUCT(ModeAndColorEnd);
	AEFX_CLR_STRUCT(GoldNoiseStart);
	AEFX_CLR_STRUCT(AllotmentGold);
	AEFX_CLR_STRUCT(MultiplierGold);
	AEFX_CLR_STRUCT(GoldNoiseEnd);
	AEFX_CLR_STRUCT(GenericNoiseStart);
	AEFX_CLR_STRUCT(AllotmentGeneric);
	AEFX_CLR_STRUCT(MultiplierGeneric);
	AEFX_CLR_STRUCT(ZoomGeneric);
	AEFX_CLR_STRUCT(GenericNoiseEnd);
	AEFX_CLR_STRUCT(ClassicPerlinNoiseStart);
	AEFX_CLR_STRUCT(AllotmentCperlin);
	AEFX_CLR_STRUCT(MultiplierCperlin);
	AEFX_CLR_STRUCT(ZoomCperlin);
	AEFX_CLR_STRUCT(ClassicPerlinNoiseEnd);
	AEFX_CLR_STRUCT(PerlinNoiseStart);
	AEFX_CLR_STRUCT(AllotmentPerlin);
	AEFX_CLR_STRUCT(MultiplierPerlin);
	AEFX_CLR_STRUCT(DimensionPerlin);
	AEFX_CLR_STRUCT(ZoomPerlin);
	AEFX_CLR_STRUCT(PerlinNoiseEnd);
	AEFX_CLR_STRUCT(SimplexNoiseStart);
	AEFX_CLR_STRUCT(AllotmentSimplex);
	AEFX_CLR_STRUCT(MultiplierSimplex);
	AEFX_CLR_STRUCT(ZoomSimplex);
	AEFX_CLR_STRUCT(SimplexNoiseEnd);
	AEFX_CLR_STRUCT(CellvoronoiStart);
	AEFX_CLR_STRUCT(AllotmentCv);
	AEFX_CLR_STRUCT(MultiplierCv);
	AEFX_CLR_STRUCT(ZoomCv);
	AEFX_CLR_STRUCT(CellvoronoiEnd);



	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MODE_AND_COLOR_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ModeAndColorStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_COLOR, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&Color));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MODE, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&Mode));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MODE_AND_COLOR_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ModeAndColorEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_GOLD_NOISE_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&GoldNoiseStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_GOLD, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentGold));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_GOLD, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierGold));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_GOLD_NOISE_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&GoldNoiseEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_GENERIC_NOISE_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&GenericNoiseStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_GENERIC, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentGeneric));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_GENERIC, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierGeneric));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ZOOM_GENERIC, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ZoomGeneric));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_GENERIC_NOISE_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&GenericNoiseEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_CLASSIC_PERLIN_NOISE_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ClassicPerlinNoiseStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_CPERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentCperlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_CPERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierCperlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ZOOM_CPERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ZoomCperlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_CLASSIC_PERLIN_NOISE_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ClassicPerlinNoiseEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_PERLIN_NOISE_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&PerlinNoiseStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_PERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentPerlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_PERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierPerlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_DIMENSION_PERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&DimensionPerlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ZOOM_PERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ZoomPerlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_PERLIN_NOISE_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&PerlinNoiseEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_SIMPLEX_NOISE_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&SimplexNoiseStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_SIMPLEX, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentSimplex));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_SIMPLEX, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierSimplex));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ZOOM_SIMPLEX, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ZoomSimplex));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_SIMPLEX_NOISE_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&SimplexNoiseEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_CELLVORONOI_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&CellvoronoiStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_CV, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentCv));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_CV, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierCv));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ZOOM_CV, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ZoomCv));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_CELLVORONOI_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&CellvoronoiEnd));

	ERR(extra->cb->checkout_layer(in_data->effect_ref,
		BEDLM_INPUT,
		BEDLM_INPUT,
		&req,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&in_result));

	if (!err){
		UnionLRect(&in_result.result_rect, &extra->output->result_rect);
		UnionLRect(&in_result.max_result_rect, &extra->output->max_result_rect);
	}
	ERR2(PF_CHECKIN_PARAM(in_data, &ModeAndColorStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &Color));
	ERR2(PF_CHECKIN_PARAM(in_data, &Mode));
	ERR2(PF_CHECKIN_PARAM(in_data, &ModeAndColorEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &GoldNoiseStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentGold));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierGold));
	ERR2(PF_CHECKIN_PARAM(in_data, &GoldNoiseEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &GenericNoiseStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentGeneric));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierGeneric));
	ERR2(PF_CHECKIN_PARAM(in_data, &ZoomGeneric));
	ERR2(PF_CHECKIN_PARAM(in_data, &GenericNoiseEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &ClassicPerlinNoiseStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentCperlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierCperlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &ZoomCperlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &ClassicPerlinNoiseEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &PerlinNoiseStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentPerlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierPerlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &DimensionPerlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &ZoomPerlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &PerlinNoiseEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &SimplexNoiseStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentSimplex));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierSimplex));
	ERR2(PF_CHECKIN_PARAM(in_data, &ZoomSimplex));
	ERR2(PF_CHECKIN_PARAM(in_data, &SimplexNoiseEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &CellvoronoiStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentCv));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierCv));
	ERR2(PF_CHECKIN_PARAM(in_data, &ZoomCv));
	ERR2(PF_CHECKIN_PARAM(in_data, &CellvoronoiEnd));
	return err;
}

static PF_Err
SmartRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_SmartRenderExtra		*extra)
{
	PF_Err				err = PF_Err_NONE,
						err2 = PF_Err_NONE;

	PF_EffectWorld		*input_worldP = NULL,
						*output_worldP = NULL;
	PF_WorldSuite2		*wsP = NULL;
	PF_PixelFormat		format = PF_PixelFormat_INVALID;
	PF_FpLong			sliderVal = 0;

	AEGP_SuiteHandler suites(in_data->pica_basicP);
	

	PF_ParamDef		ModeAndColorStart;
	PF_ParamDef		Color;
	PF_ParamDef		Mode;
	PF_ParamDef		ModeAndColorEnd;
	PF_ParamDef		GoldNoiseStart;
	PF_ParamDef		AllotmentGold;
	PF_ParamDef		MultiplierGold;
	PF_ParamDef		GoldNoiseEnd;
	PF_ParamDef		GenericNoiseStart;
	PF_ParamDef		AllotmentGeneric;
	PF_ParamDef		MultiplierGeneric;
	PF_ParamDef		ZoomGeneric;
	PF_ParamDef		GenericNoiseEnd;
	PF_ParamDef		ClassicPerlinNoiseStart;
	PF_ParamDef		AllotmentCperlin;
	PF_ParamDef		MultiplierCperlin;
	PF_ParamDef		ZoomCperlin;
	PF_ParamDef		ClassicPerlinNoiseEnd;
	PF_ParamDef		PerlinNoiseStart;
	PF_ParamDef		AllotmentPerlin;
	PF_ParamDef		MultiplierPerlin;
	PF_ParamDef		DimensionPerlin;
	PF_ParamDef		ZoomPerlin;
	PF_ParamDef		PerlinNoiseEnd;
	PF_ParamDef		SimplexNoiseStart;
	PF_ParamDef		AllotmentSimplex;
	PF_ParamDef		MultiplierSimplex;
	PF_ParamDef		ZoomSimplex;
	PF_ParamDef		SimplexNoiseEnd;
	PF_ParamDef		CellvoronoiStart;
	PF_ParamDef		AllotmentCv;
	PF_ParamDef		MultiplierCv;
	PF_ParamDef		ZoomCv;
	PF_ParamDef		CellvoronoiEnd;

	AEFX_CLR_STRUCT(ModeAndColorStart);
	AEFX_CLR_STRUCT(Color);
	AEFX_CLR_STRUCT(Mode);
	AEFX_CLR_STRUCT(ModeAndColorEnd);
	AEFX_CLR_STRUCT(GoldNoiseStart);
	AEFX_CLR_STRUCT(AllotmentGold);
	AEFX_CLR_STRUCT(MultiplierGold);
	AEFX_CLR_STRUCT(GoldNoiseEnd);
	AEFX_CLR_STRUCT(GenericNoiseStart);
	AEFX_CLR_STRUCT(AllotmentGeneric);
	AEFX_CLR_STRUCT(MultiplierGeneric);
	AEFX_CLR_STRUCT(ZoomGeneric);
	AEFX_CLR_STRUCT(GenericNoiseEnd);
	AEFX_CLR_STRUCT(ClassicPerlinNoiseStart);
	AEFX_CLR_STRUCT(AllotmentCperlin);
	AEFX_CLR_STRUCT(MultiplierCperlin);
	AEFX_CLR_STRUCT(ZoomCperlin);
	AEFX_CLR_STRUCT(ClassicPerlinNoiseEnd);
	AEFX_CLR_STRUCT(PerlinNoiseStart);
	AEFX_CLR_STRUCT(AllotmentPerlin);
	AEFX_CLR_STRUCT(MultiplierPerlin);
	AEFX_CLR_STRUCT(DimensionPerlin);
	AEFX_CLR_STRUCT(ZoomPerlin);
	AEFX_CLR_STRUCT(PerlinNoiseEnd);
	AEFX_CLR_STRUCT(SimplexNoiseStart);
	AEFX_CLR_STRUCT(AllotmentSimplex);
	AEFX_CLR_STRUCT(MultiplierSimplex);
	AEFX_CLR_STRUCT(ZoomSimplex);
	AEFX_CLR_STRUCT(SimplexNoiseEnd);
	AEFX_CLR_STRUCT(CellvoronoiStart);
	AEFX_CLR_STRUCT(AllotmentCv);
	AEFX_CLR_STRUCT(MultiplierCv);
	AEFX_CLR_STRUCT(ZoomCv);
	AEFX_CLR_STRUCT(CellvoronoiEnd);



	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MODE_AND_COLOR_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ModeAndColorStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_COLOR, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&Color));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MODE, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&Mode));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MODE_AND_COLOR_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ModeAndColorEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_GOLD_NOISE_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&GoldNoiseStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_GOLD, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentGold));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_GOLD, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierGold));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_GOLD_NOISE_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&GoldNoiseEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_GENERIC_NOISE_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&GenericNoiseStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_GENERIC, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentGeneric));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_GENERIC, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierGeneric));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ZOOM_GENERIC, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ZoomGeneric));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_GENERIC_NOISE_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&GenericNoiseEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_CLASSIC_PERLIN_NOISE_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ClassicPerlinNoiseStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_CPERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentCperlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_CPERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierCperlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ZOOM_CPERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ZoomCperlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_CLASSIC_PERLIN_NOISE_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ClassicPerlinNoiseEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_PERLIN_NOISE_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&PerlinNoiseStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_PERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentPerlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_PERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierPerlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_DIMENSION_PERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&DimensionPerlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ZOOM_PERLIN, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ZoomPerlin));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_PERLIN_NOISE_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&PerlinNoiseEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_SIMPLEX_NOISE_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&SimplexNoiseStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_SIMPLEX, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentSimplex));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_SIMPLEX, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierSimplex));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ZOOM_SIMPLEX, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ZoomSimplex));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_SIMPLEX_NOISE_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&SimplexNoiseEnd));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_CELLVORONOI_START, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&CellvoronoiStart));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ALLOTMENT_CV, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&AllotmentCv));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_MULTIPLIER_CV, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&MultiplierCv));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_ZOOM_CV, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&ZoomCv));
	ERR(PF_CHECKOUT_PARAM(in_data,
		BDLM_CELLVORONOI_END, in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&CellvoronoiEnd));
	
	PF_PixelFloat Color_Val;
	A_long Mode_Val;
	PF_FpLong AllotmentGold_Val;
	PF_Fixed MultiplierGold_Val;
	PF_FpLong AllotmentGeneric_Val;
	PF_Fixed MultiplierGeneric_Val;
	PF_FpLong ZoomGeneric_Val;
	PF_FpLong AllotmentCperlin_Val;
	PF_Fixed MultiplierCperlin_Val;
	PF_FpLong ZoomCperlin_Val;
	PF_FpLong AllotmentPerlin_Val;
	PF_FpLong DimensionPerlin_Val;
	PF_Fixed MultiplierPerlin_Val;
	PF_FpLong ZoomPerlin_Val;
	PF_FpLong AllotmentSimplex_Val;
	PF_Fixed MultiplierSimplex_Val;
	PF_FpLong ZoomSimplex_Val;
	PF_FpLong AllotmentCv_Val;
	PF_Fixed MultiplierCv_Val;
	PF_FpLong ZoomCv_Val;


	if (!err){
		
		suites.ColorParamSuite1()->PF_GetFloatingPointColorFromColorDef(in_data->effect_ref,
			&Color, &Color_Val);
		Mode_Val = Mode.u.pd.value;
		AllotmentGold_Val = AllotmentGold.u.fs_d.value;
		MultiplierGold_Val = MultiplierGold.u.ad.value;
		AllotmentGeneric_Val = AllotmentGeneric.u.fs_d.value;
		MultiplierGeneric_Val = MultiplierGeneric.u.ad.value;
		ZoomGeneric_Val = ZoomGeneric.u.fs_d.value;
		AllotmentCperlin_Val = AllotmentCperlin.u.fs_d.value;
		MultiplierCperlin_Val = MultiplierCperlin.u.ad.value;
		ZoomCperlin_Val = ZoomCperlin.u.fs_d.value;
		AllotmentPerlin_Val = AllotmentPerlin.u.fs_d.value;
		MultiplierPerlin_Val = MultiplierPerlin.u.ad.value;
		DimensionPerlin_Val = DimensionPerlin.u.fs_d.value;
		ZoomPerlin_Val = ZoomPerlin.u.fs_d.value;
		AllotmentSimplex_Val = AllotmentSimplex.u.fs_d.value;
		MultiplierSimplex_Val = MultiplierSimplex.u.ad.value;
		ZoomSimplex_Val = ZoomSimplex.u.fs_d.value;
		AllotmentCv_Val = AllotmentCv.u.fs_d.value;
		MultiplierCv_Val = MultiplierCv.u.ad.value;
		ZoomCv_Val = ZoomCv.u.fs_d.value;

	}

	Noise Gold_Noise((float)Color_Val.red, (float)Color_Val.green, (float)Color_Val.blue,
		AllotmentGold_Val, MultiplierGold_Val, 0.0, 0.0);
	Gold_Noise.mode = (Mode_Val == 1) ? 1.00 : 0.0;
	Noise Generic_Noise((float)Color_Val.red, (float)Color_Val.green, (float)Color_Val.blue,
		AllotmentGeneric_Val, MultiplierGeneric_Val, 0.0, ZoomGeneric_Val);
	Generic_Noise.mode = (Mode_Val == 2) ? 1.00 : 0.0;
	Noise CPerlin_Noise((float)Color_Val.red, (float)Color_Val.green, (float)Color_Val.blue,
		AllotmentCperlin_Val, MultiplierCperlin_Val, 0.0, ZoomCperlin_Val);
	CPerlin_Noise.mode = (Mode_Val == 3) ? 1.00 : 0.0;
	Noise Perlin_Noise((float)Color_Val.red, (float)Color_Val.green, (float)Color_Val.blue,
		AllotmentPerlin_Val, MultiplierPerlin_Val, DimensionPerlin_Val, ZoomPerlin_Val);
	Perlin_Noise.mode = (Mode_Val == 4) ? 1.00 : 0.0;
	Noise Simplex_Noise((float)Color_Val.red, (float)Color_Val.green, (float)Color_Val.blue,
		AllotmentSimplex_Val, MultiplierSimplex_Val, 0.0, ZoomSimplex_Val);
	Simplex_Noise.mode = (Mode_Val == 5) ? 1.00 : 0.0;
	Noise Voronoi_Noise((float)Color_Val.red, (float)Color_Val.green, (float)Color_Val.blue,
		AllotmentCv_Val, MultiplierCv_Val, 0.0, ZoomCv_Val);
	Voronoi_Noise.mode = (Mode_Val == 6) ? 1.00 : 0.0;

	std::map<std::string, Noise> noises;

	noises["Gold Noise"] = Gold_Noise;
	noises["Generic Noise"] = Generic_Noise;
	noises["Classic Perlin"] = CPerlin_Noise;
	noises["Perlin Noise"] = Perlin_Noise;
	noises["Simplex Noise"] = Simplex_Noise;
	noises["Voronoi Noise"] = Voronoi_Noise;

	ERR((extra->cb->checkout_layer_pixels(in_data->effect_ref, BEDLM_INPUT, &input_worldP)));

	ERR(extra->cb->checkout_output(in_data->effect_ref, &output_worldP));

	ERR(AEFX_AcquireSuite(in_data,
		out_data,
		kPFWorldSuite,
		kPFWorldSuiteVersion2,
		"Couldn't load suite.",
		(void**)&wsP));

	if (!err){
		try
		{
			// always restore back AE's own OGL context
			SaveRestoreOGLContext oSavedContext;

			// our render specific context (one per thread)
			AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr renderContext = GetCurrentRenderContext();

			if (!renderContext->mInitialized) {
				//Now comes the OpenGL part - OS specific loading to start with
				AESDK_OpenGL_Startup(*renderContext.get(), S_GLator_EffectCommonData.get());

				renderContext->mInitialized = true;
			}

			renderContext->SetPluginContext();
			
			// - Gremedy OpenGL debugger
			// - Example of using a OpenGL extension
			bool hasGremedy = renderContext->mExtensions.find(gl::GLextension::GL_GREMEDY_frame_terminator) != renderContext->mExtensions.end();

			A_long				widthL = input_worldP->width;
			A_long				heightL = input_worldP->height;

			//loading OpenGL resources
			AESDK_OpenGL_InitResources(*renderContext.get(), widthL, heightL, S_ResourcePath);

			CHECK(wsP->PF_GetPixelFormat(input_worldP, &format));

			// upload the input world to a texture
			size_t pixSize;
			gl::GLenum glFmt;
			float multiplier16bit;
			gl::GLuint inputFrameTexture = UploadTexture(suites, format, input_worldP, output_worldP, in_data, pixSize, glFmt, multiplier16bit);
			
			// Set up the frame-buffer object just like a window.
			AESDK_OpenGL_MakeReadyToRender(*renderContext.get(), renderContext->mOutputFrameTexture);
			ReportIfErrorFramebuffer(in_data, out_data);

			glViewport(0, 0, widthL, heightL);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			
			// - simply blend the texture inside the frame buffer
			// - TODO: hack your own shader there
			RenderGL(renderContext, widthL, heightL, inputFrameTexture, noises, multiplier16bit);

			// - we toggle PBO textures (we use the PBO we just created as an input)
			AESDK_OpenGL_MakeReadyToRender(*renderContext.get(), inputFrameTexture);
			ReportIfErrorFramebuffer(in_data, out_data);

			glClear(GL_COLOR_BUFFER_BIT);

			// swizzle using the previous output
			SwizzleGL(renderContext, widthL, heightL, renderContext->mOutputFrameTexture, noises, multiplier16bit);

			if (hasGremedy) {
				gl::glFrameTerminatorGREMEDY();
			}

			// - get back to CPU the result, and inside the output world
			DownloadTexture(renderContext, suites, input_worldP, output_worldP, in_data,
				format, pixSize, glFmt);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDeleteTextures(1, &inputFrameTexture);
		}
		catch (PF_Err& thrown_err)
		{
			err = thrown_err;
		}
		catch (...)
		{
			err = PF_Err_OUT_OF_MEMORY;
		}
	}

	// If you have PF_ABORT or PF_PROG higher up, you must set
	// the AE context back before calling them, and then take it back again
	// if you want to call some more OpenGL.		
	ERR(PF_ABORT(in_data));

	ERR2(AEFX_ReleaseSuite(in_data,
		out_data,
		kPFWorldSuite,
		kPFWorldSuiteVersion2,
		"Couldn't release suite."));
	ERR2(PF_CHECKIN_PARAM(in_data, &ModeAndColorStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &Color));
	ERR2(PF_CHECKIN_PARAM(in_data, &Mode));
	ERR2(PF_CHECKIN_PARAM(in_data, &ModeAndColorEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &GoldNoiseStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentGold));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierGold));
	ERR2(PF_CHECKIN_PARAM(in_data, &GoldNoiseEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &GenericNoiseStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentGeneric));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierGeneric));
	ERR2(PF_CHECKIN_PARAM(in_data, &ZoomGeneric));
	ERR2(PF_CHECKIN_PARAM(in_data, &GenericNoiseEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &ClassicPerlinNoiseStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentCperlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierCperlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &ZoomCperlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &ClassicPerlinNoiseEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &PerlinNoiseStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentPerlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierPerlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &DimensionPerlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &ZoomPerlin));
	ERR2(PF_CHECKIN_PARAM(in_data, &PerlinNoiseEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &SimplexNoiseStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentSimplex));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierSimplex));
	ERR2(PF_CHECKIN_PARAM(in_data, &ZoomSimplex));
	ERR2(PF_CHECKIN_PARAM(in_data, &SimplexNoiseEnd));
	ERR2(PF_CHECKIN_PARAM(in_data, &CellvoronoiStart));
	ERR2(PF_CHECKIN_PARAM(in_data, &AllotmentCv));
	ERR2(PF_CHECKIN_PARAM(in_data, &MultiplierCv));
	ERR2(PF_CHECKIN_PARAM(in_data, &ZoomCv));
	ERR2(PF_CHECKIN_PARAM(in_data, &CellvoronoiEnd));
	ERR2(extra->cb->checkin_layer_pixels(in_data->effect_ref, BEDLM_INPUT));

	return err;
}


extern "C" DllExport
PF_Err PluginDataEntryFunction(
	PF_PluginDataPtr inPtr,
	PF_PluginDataCB inPluginDataCallBackPtr,
	SPBasicSuite* inSPBasicSuitePtr,
	const char* inHostName,
	const char* inHostVersion)
{
	PF_Err result = PF_Err_INVALID_CALLBACK;

	result = PF_REGISTER_EFFECT(
		inPtr,
		inPluginDataCallBackPtr,
		"GLator", // Name
		"ADBE GLator", // Match Name
		"Sample Plug-ins", // Category
		AE_RESERVED_INFO); // Reserved Info

	return result;
}


PF_Err
EffectMain(
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra)
{
	PF_Err		err = PF_Err_NONE;
	
	try {
		switch (cmd) {
			case PF_Cmd_ABOUT:
				err = About(in_data,
							out_data,
							params,
							output);
				break;
				
			case PF_Cmd_GLOBAL_SETUP:
				err = GlobalSetup(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_GLOBAL_SETDOWN:
				err = GlobalSetdown(	in_data,
										out_data,
										params,
										output);
				break;

			case  PF_Cmd_SMART_PRE_RENDER:
				err = PreRender(in_data, out_data, reinterpret_cast<PF_PreRenderExtra*>(extra));
				break;

			case  PF_Cmd_SMART_RENDER:
				err = SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra*>(extra));
				break;
		}
	}
	catch(PF_Err &thrown_err){
		err = thrown_err;
	}
	return err;
}
