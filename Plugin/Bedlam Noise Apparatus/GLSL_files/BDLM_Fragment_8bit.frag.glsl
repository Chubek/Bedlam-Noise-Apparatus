#version 330
uniform sampler2D videoTexture;
uniform float multiplier16bit;
in vec4 out_pos;
in vec2 out_uvs;
out vec4 colourOut;


struct Noise
{
	float R;
	float B;
	float G;
	float allot;
	float mult;
	float zoom;
	float dim;
	float mode;
} noise_struct;

uniform Noise Gold;
uniform Noise Generic;
uniform Noise CPerlin;
uniform Noise Perlin;
uniform Noise Simplex;
uniform Noise Voronoi;

//Gold Noise Vars
float PHI = 1.61803398874989484820459 * 00000.1; // Golden Ratio   
float PI  = 3.14159265358979323846264 * 00000.1; // PI
float SQ2 = 1.41421356237309504880169 * 10000.0; // Square Root of Two

////////////////////
//Gold Noise_START//
////////////////////

float gold_noise(in vec2 coordinate, in float seed){
    return fract(tan(distance(coordinate*(seed+PHI), vec2(PHI, PI)))*SQ2);
}

///////////////////
//Gold Noise_END//
//////////////////


void main( void )
{
	//simplest texture lookup
	colourOut = texture( videoTexture, out_uvs.xy ); 
	// in case of 16 bits, convert 32768->65535
	colourOut = colourOut * multiplier16bit;
	// swizzle ARGB to RGBA
	colourOut = vec4(colourOut.g, colourOut.b, colourOut.a, colourOut.r);
	// convert to pre-multiplied alpha
	colourOut = vec4(colourOut.a * colourOut.r, colourOut.a * colourOut.g, colourOut.a * colourOut.b, colourOut.a);

	
	vec3 Gold_Noise = vec3(Gold.R, Gold.G, Gold.B) * vec3(gold_noise(out_uvs, Gold.allot * sin(Gold.mult))) * Gold.mode;


	colourOut = vec4(Gold_Noise, 1.0);


}
