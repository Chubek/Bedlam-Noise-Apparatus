#version 330
#define M_PI 3.14159265358979323846
#define OCTAVES   		1
#define SWITCH_TIME 	60.0
#define SCALE		   10.0		
#define BIAS   		   +0.0
#define POWER			1.0		
#define WARP_INTENSITY	0.00	
#define WARP_FREQUENCY	16.0

//WARNING: DEAR END-USER, PLEASE DO_NOT MODIFY THIS FILE. IF THIS FILE BECOMES DAMAGED OR ANYTHING HAPPENS TO IT, THE PLUGIN
//WON'T RUN. WHY HAVEN'T I BINNED IT? BECAUSE I DIDN'T FEEL LIKE ADDING 3 MEGABYTES TO THE TOTAL SIZE OF THE PLUGIN, THAT'S WHY!

uniform sampler2D videoTexture;
uniform float multiplier16bit;
uniform float opacity;
uniform float blend;
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

//Generic Noise Funcs
float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 mod289(vec4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 perm(vec4 x){return mod289(((x * 34.0) + 1.0) * x);}


//Classic Perlin Noise Funcs
vec4 permute_cp(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec4 taylorInvSqrt_cp(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
vec3 fade_cp(vec3 t) {return t*t*t*(t*(t*6.0-15.0)+10.0);}

//Perlin Noise Funcs
float perlin_rand(vec2 co){return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);}
float perlin_rand (vec2 co, float l) {return perlin_rand(vec2(perlin_rand(co), l));}
float perlin_rand (vec2 co, float l, float t) {return perlin_rand(vec2(perlin_rand(co, l), t));}


//Simplex Noise Funcs
vec4 permute_simplex(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec4 taylorInvSqrt_Simplex(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}


//Voronoi Noise Funcs
float t = Voronoi.allot * Voronoi.mult * Voronoi.mode/SWITCH_TIME;

float function 			= mod(t,4.0);
bool  multiply_by_F1	= mod(t,8.0)  >= 4.0;
bool  inverse				= mod(t,16.0) >= 8.0;
float distance_type	= mod(t/16.0,4.0);


vec2 hash( vec2 p ){
	p = vec2( dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3)));
	return fract(sin(p)*43758.5453);
}

////////////////////
//Gold Noise_START//
////////////////////

float gold_noise(in vec2 coordinate, in float seed){
    return fract(tan(distance(coordinate*(seed+PHI), vec2(PHI, PI)))*SQ2);
}

///////////////////
//Gold Noise_END//
//////////////////


/////////////////////////////
//////Generic_Noise_Start////
/////////////////////////////

float generic_noise(vec3 p){
    vec3 a = floor(p);
    vec3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);

    vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
    vec4 k1 = perm(b.xyxy);
    vec4 k2 = perm(k1.xyxy + b.zzww);

    vec4 c = k2 + a.zzzz;
    vec4 k3 = perm(c);
    vec4 k4 = perm(c + 1.0);

    vec4 o1 = fract(k3 * (1.0 / 41.0));
    vec4 o2 = fract(k4 * (1.0 / 41.0));

    vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

    return o4.y * d.y + o4.x * (1.0 - d.y);
}

/////////////////////////////
///////Generic_Noise_End/////
/////////////////////////////


/////////////////////////////
////Classic_Perlin_Start/////
/////////////////////////////

float cnoise(vec3 P){
  vec3 Pi0 = floor(P); // Integer part for indexing
  vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
  Pi0 = mod(Pi0, 289.0);
  Pi1 = mod(Pi1, 289.0);
  vec3 Pf0 = fract(P); // Fractional part for interpolation
  vec3 Pf1 = Pf0 - vec3(1.0); // Fractional part - 1.0
  vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
  vec4 iy = vec4(Pi0.yy, Pi1.yy);
  vec4 iz0 = Pi0.zzzz;
  vec4 iz1 = Pi1.zzzz;

  vec4 ixy = permute_cp(permute_cp(ix) + iy);
  vec4 ixy0 = permute_cp(ixy + iz0);
  vec4 ixy1 = permute_cp(ixy + iz1);

  vec4 gx0 = ixy0 / 7.0;
  vec4 gy0 = fract(floor(gx0) / 7.0) - 0.5;
  gx0 = fract(gx0);
  vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
  vec4 sz0 = step(gz0, vec4(0.0));
  gx0 -= sz0 * (step(0.0, gx0) - 0.5);
  gy0 -= sz0 * (step(0.0, gy0) - 0.5);

  vec4 gx1 = ixy1 / 7.0;
  vec4 gy1 = fract(floor(gx1) / 7.0) - 0.5;
  gx1 = fract(gx1);
  vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
  vec4 sz1 = step(gz1, vec4(0.0));
  gx1 -= sz1 * (step(0.0, gx1) - 0.5);
  gy1 -= sz1 * (step(0.0, gy1) - 0.5);

  vec3 g000 = vec3(gx0.x,gy0.x,gz0.x);
  vec3 g100 = vec3(gx0.y,gy0.y,gz0.y);
  vec3 g010 = vec3(gx0.z,gy0.z,gz0.z);
  vec3 g110 = vec3(gx0.w,gy0.w,gz0.w);
  vec3 g001 = vec3(gx1.x,gy1.x,gz1.x);
  vec3 g101 = vec3(gx1.y,gy1.y,gz1.y);
  vec3 g011 = vec3(gx1.z,gy1.z,gz1.z);
  vec3 g111 = vec3(gx1.w,gy1.w,gz1.w);

  vec4 norm0 = taylorInvSqrt_cp(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
  g000 *= norm0.x;
  g010 *= norm0.y;
  g100 *= norm0.z;
  g110 *= norm0.w;
  vec4 norm1 = taylorInvSqrt_cp(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
  g001 *= norm1.x;
  g011 *= norm1.y;
  g101 *= norm1.z;
  g111 *= norm1.w;

  float n000 = dot(g000, Pf0);
  float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
  float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
  float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
  float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
  float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
  float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
  float n111 = dot(g111, Pf1);

  vec3 fade_cp_xyz = fade_cp(Pf0);
  vec4 n_z = mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_cp_xyz.z);
  vec2 n_yz = mix(n_z.xy, n_z.zw, fade_cp_xyz.y);
  float n_xyz = mix(n_yz.x, n_yz.y, fade_cp_xyz.x); 
  return 2.2 * n_xyz;
}

///////////////////////////////////////
/////////Classic_Perlin_End///////////
///////////////////////////////////////


//////////////////////////////////////
////////////Perlin_Start/////////////
////////////////////////////////////

float perlin(vec2 p, float dim, float time) {
	vec2 pos = floor(p * dim);
	vec2 posx = pos + vec2(1.0, 0.0);
	vec2 posy = pos + vec2(0.0, 1.0);
	vec2 posxy = pos + vec2(1.0);
	
	float c = perlin_rand(pos, dim, time);
	float cx = perlin_rand(posx, dim, time);
	float cy = perlin_rand(posy, dim, time);
	float cxy = perlin_rand(posxy, dim, time);
	
	vec2 d = fract(p * dim);
	d = -0.5 * cos(d * M_PI) + 0.5;
	
	float ccx = mix(c, cx, d.x);
	float cycxy = mix(cy, cxy, d.x);
	float center = mix(ccx, cycxy, d.y);
	
	return center * 2.0 - 1.0;
}


//////////////////////////////////////////
///////////////Perlin_End////////////////
/////////////////////////////////////////


////////////////////////////////////////
/////////////SIMEPLX_START/////////////
/////////////////////////////////////

float snoise(vec3 v){ 
  const vec2  C = vec2(1.0/6.0, 1.0/3.0) ;
  const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);

// First corner
  vec3 i  = floor(v + dot(v, C.yyy) );
  vec3 x0 =   v - i + dot(i, C.xxx) ;

// Other corners
  vec3 g = step(x0.yzx, x0.xyz);
  vec3 l = 1.0 - g;
  vec3 i1 = min( g.xyz, l.zxy );
  vec3 i2 = max( g.xyz, l.zxy );

  //  x0 = x0 - 0. + 0.0 * C 
  vec3 x1 = x0 - i1 + 1.0 * C.xxx;
  vec3 x2 = x0 - i2 + 2.0 * C.xxx;
  vec3 x3 = x0 - 1. + 3.0 * C.xxx;

// Permutations
  i = mod(i, 289.0 ); 
  vec4 p = permute_simplex( permute_simplex( permute_simplex( 
             i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
           + i.y + vec4(0.0, i1.y, i2.y, 1.0 )) 
           + i.x + vec4(0.0, i1.x, i2.x, 1.0 ));

// Gradients
// ( N*N points uniformly over a square, mapped onto an octahedron.)
  float n_ = 1.0/7.0; // N=7
  vec3  ns = n_ * D.wyz - D.xzx;

  vec4 j = p - 49.0 * floor(p * ns.z *ns.z);  //  mod(p,N*N)

  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7.0 * x_ );    // mod(j,N)

  vec4 x = x_ *ns.x + ns.yyyy;
  vec4 y = y_ *ns.x + ns.yyyy;
  vec4 h = 1.0 - abs(x) - abs(y);

  vec4 b0 = vec4( x.xy, y.xy );
  vec4 b1 = vec4( x.zw, y.zw );

  vec4 s0 = floor(b0)*2.0 + 1.0;
  vec4 s1 = floor(b1)*2.0 + 1.0;
  vec4 sh = -step(h, vec4(0.0));

  vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
  vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;

  vec3 p0 = vec3(a0.xy,h.x);
  vec3 p1 = vec3(a0.zw,h.y);
  vec3 p2 = vec3(a1.xy,h.z);
  vec3 p3 = vec3(a1.zw,h.w);

//Normalise gradients
  vec4 norm = taylorInvSqrt_Simplex(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;

// Mix final noise value
  vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
  m = m * m;
  return 42.0 * dot( m*m, vec4( dot(p0,x0), dot(p1,x1), 
                                dot(p2,x2), dot(p3,x3) ) );
}

///////////////////////////////////////////
/////////////////SIMPLEX_END///////////////
///////////////////////////////////////////


///////////////////////////////////////////
/////////////Voronoi_START/////////////////
///////////////////////////////////////////


float voronoi( in vec2 x ){
	vec2 n = floor( x );
	vec2 f = fract( x );
	
	float F1 = 8.0;
	float F2 = 8.0;
	
	for( int j=-1; j<=1; j++ )
		for( int i=-1; i<=1; i++ ){
			vec2 g = vec2(i,j);
			vec2 o = hash( n + g );

			o = 0.5 + 0.41*sin( t + 6.2831*o );	
			vec2 r = g - f + o;

		float d = 	distance_type < 1.0 ? dot(r,r)  :				// euclidean^2
				  	distance_type < 2.0 ? sqrt(dot(r,r)) :			// euclidean
					distance_type < 3.0 ? abs(r.x) + abs(r.y) :		// manhattan
					distance_type < 4.0 ? max(abs(r.x), abs(r.y)) :	// chebyshev
					0.0;

		if( d<F1 ) { 
			F2 = F1; 
			F1 = d; 
		} else if( d<F2 ) {
			F2 = d;
		}
    }
	
	float c = function < 1.0 ? F1 : 
			  function < 2.0 ? F2 : 
			  function < 3.0 ? F2-F1 :
			  function < 4.0 ? (F1+F2)/2.0 : 
			  0.0;
		
	if( multiply_by_F1 )	c *= F1;
	if( inverse )			c = 1.0 - c;
	
    return c;
}

float fbm( in vec2 p ){
	float s = 0.0;
	float m = 0.0;
	float a = 0.5;
	
	for( int i=0; i<OCTAVES; i++ ){
		s += a * voronoi(p);
		m += a;
		a *= 0.5;
		p *= 2.0;
	}
	return s/m;
}


//////////////////////////////////////////////////////
///////////////////Voronoi_End///////////////////////
////////////////////////////////////////////////////




////////////////ACKNOWLEDGEMENT//////////
//I have, in fact, based a few codes on//
//ShaderToy shaders. Refer to the About//
//Window for more info. Many thanks....//
///////////~Chubak//////////////////////
////////////////////////////////////////





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

	
	vec3 Gold_Noise = vec3(Gold.R, Gold.G, Gold.B) * vec3(gold_noise(out_uvs, Gold.allot * sin(Gold.mult) * Gold.mode)) * Gold.mode;
	
	vec3 Generic_Pos = vec3(out_uvs, sin(Generic.allot) * sin(Generic.mult)*  Generic.mode);
	vec3 Generic_Noise = vec3(Generic.R, Generic.G, Generic.B) *
	generic_noise(Generic_Pos * Generic.zoom * 2 * Generic.mode) * Generic.mode;

	vec3 CPerlin_Pos = vec3(out_uvs, sin(CPerlin.allot) * sin(CPerlin.mult)* CPerlin.mode);
	vec3 CPerlin_Noise = vec3(CPerlin.R, CPerlin.G, CPerlin.B) *
	cnoise(CPerlin_Pos * CPerlin.zoom * 2* CPerlin.mode) * CPerlin.mode;

	vec3 Perlin_Noise = vec3(Perlin.R, Perlin.G, Perlin.B) * perlin(out_uvs, Perlin.dim* Perlin.zoom * Perlin.mode,
	Perlin.allot * Perlin.mult * Perlin.mode) * Perlin.mode;

	vec3 Simplex_Pos = vec3(out_uvs, sin(Simplex.allot) * sin(Simplex.mult) * Simplex.mode);
	vec3 Simplex_Noise = vec3(Simplex.R, Simplex.G, Simplex.B) * snoise(Simplex_Pos * Simplex.zoom * Simplex.mode) * Simplex.mode;
	
	vec3 Voronoi_Noise = vec3(Voronoi.R, Voronoi.G, Voronoi.B) * POWER * fbm(SCALE*Voronoi.zoom * Voronoi.mode * out_uvs) + BIAS;


	vec4 final_noise = vec4(Gold_Noise 
	+ Generic_Noise 
	+ CPerlin_Noise 
	+ Perlin_Noise
	+ Simplex_Noise
	+ Voronoi_Noise, opacity);

	colourOut = (blend == 1.0) ? colourOut + final_noise : colourOut * final_noise;


}
