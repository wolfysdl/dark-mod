/*****************************************************************************
                    The Dark Mod GPL Source Code
 
 This file is part of the The Dark Mod Source Code, originally based 
 on the Doom 3 GPL Source Code as published in 2011.
 
 The Dark Mod Source Code is free software: you can redistribute it 
 and/or modify it under the terms of the GNU General Public License as 
 published by the Free Software Foundation, either version 3 of the License, 
 or (at your option) any later version. For details, see LICENSE.TXT.
 
 Project: The Dark Mod (http://www.thedarkmod.com/)
 
******************************************************************************/

#include "precompiled.h"
#pragma hdrstop



#include "tr_local.h"

#define	DEFAULT_SIZE		16
#define	NORMAL_MAP_SIZE		32
#define FOG_SIZE			128
#define	RAMP_RANGE			8.0
#define	DEEP_RANGE			-30.0
#define QUADRATIC_WIDTH		32
#define QUADRATIC_HEIGHT	4
#define BORDER_CLAMP_SIZE	32

#define LOAD_KEY_IMAGE_GRANULARITY 10 // grayman #3763

const char *imageFilter[] = {
	"GL_LINEAR_MIPMAP_NEAREST",
	"GL_LINEAR_MIPMAP_LINEAR",
	"GL_NEAREST",
	"GL_LINEAR",
	"GL_NEAREST_MIPMAP_NEAREST",
	"GL_NEAREST_MIPMAP_LINEAR",
	NULL
};

idCVar idImageManager::image_filter( "image_filter", imageFilter[1], CVAR_RENDERER | CVAR_ARCHIVE, "changes texture filtering on mipmapped images", imageFilter, idCmdSystem::ArgCompletion_String<imageFilter> );
idCVar idImageManager::image_anisotropy( "image_anisotropy", "1", CVAR_RENDERER | CVAR_ARCHIVE, "set the maximum texture anisotropy if available" );
idCVar idImageManager::image_lodbias( "image_lodbias", "0", CVAR_RENDERER | CVAR_ARCHIVE, "change lod bias on mipmapped images" );
idCVar idImageManager::image_downSize( "image_downSize", "0", CVAR_RENDERER | CVAR_ARCHIVE, "controls texture downsampling" );
idCVar idImageManager::image_forceDownSize( "image_forceDownSize", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "" );
idCVar idImageManager::image_roundDown( "image_roundDown", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "round bad sizes down to nearest power of two" );
idCVar idImageManager::image_colorMipLevels( "image_colorMipLevels", "0", CVAR_RENDERER | CVAR_BOOL, "development aid to see texture mip usage" );
idCVar idImageManager::image_preload( "image_preload", "1", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "if 0, dynamically load all images" );
idCVar idImageManager::image_useCompression( "image_useCompression", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "1 = load compressed (DDS) images, 0 = force everything to high quality. 0 does not work for TDM as all our textures are DDS." );
idCVar idImageManager::image_useAllFormats( "image_useAllFormats", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "allow alpha/intensity/luminance/luminance+alpha" );
idCVar idImageManager::image_useNormalCompression( "image_useNormalCompression", "1", CVAR_RENDERER | CVAR_ARCHIVE, "use rxgb/rgtc compression for normal maps if available, 0 = no, 1 - GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 2 = GL_COMPRESSED_RG_RGTC2" );
idCVar idImageManager::image_usePrecompressedTextures( "image_usePrecompressedTextures", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "Use .dds files if present." );
idCVar idImageManager::image_writePrecompressedTextures( "image_writePrecompressedTextures", "0", CVAR_RENDERER | CVAR_BOOL, "write .dds files if necessary" );
idCVar idImageManager::image_writeNormalTGA( "image_writeNormalTGA", "0", CVAR_RENDERER | CVAR_BOOL, "write .tgas of the final normal maps for debugging" );
idCVar idImageManager::image_writeTGA( "image_writeTGA", "0", CVAR_RENDERER | CVAR_BOOL, "write .tgas of the non normal maps for debugging" );
idCVar idImageManager::image_useOffLineCompression( "image_useOfflineCompression", "0", CVAR_RENDERER | CVAR_BOOL, "write a batch file for offline compression of DDS files" );
idCVar idImageManager::image_cacheMinK( "image_cacheMinK", "200", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "maximum KB of precompressed files to read at specification time" );
idCVar idImageManager::image_cacheMegs( "image_cacheMegs", "20", CVAR_RENDERER | CVAR_ARCHIVE, "maximum MB set aside for temporary loading of full-sized precompressed images" );
idCVar idImageManager::image_useCache( "image_useCache", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "1 = do background load image caching" );
idCVar idImageManager::image_showBackgroundLoads( "image_showBackgroundLoads", "0", CVAR_RENDERER | CVAR_BOOL, "1 = print number of outstanding background loads" );
idCVar idImageManager::image_downSizeSpecular( "image_downSizeSpecular", "0", CVAR_RENDERER | CVAR_ARCHIVE, "controls specular downsampling" );
idCVar idImageManager::image_downSizeBump( "image_downSizeBump", "0", CVAR_RENDERER | CVAR_ARCHIVE, "controls normal map downsampling" );
idCVar idImageManager::image_downSizeSpecularLimit( "image_downSizeSpecularLimit", "64", CVAR_RENDERER | CVAR_ARCHIVE, "controls specular downsampled limit" );
idCVar idImageManager::image_downSizeBumpLimit( "image_downSizeBumpLimit", "128", CVAR_RENDERER | CVAR_ARCHIVE, "controls normal map downsample limit" );
idCVar idImageManager::image_ignoreHighQuality( "image_ignoreHighQuality", "0", CVAR_RENDERER | CVAR_ARCHIVE, "ignore high quality setting on materials" );
idCVar idImageManager::image_downSizeLimit( "image_downSizeLimit", "256", CVAR_RENDERER | CVAR_ARCHIVE, "controls diffuse map downsample limit" ); 
idCVar idImageManager::image_blockChecksum("image_blockChecksum", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "Perform MD4 block checksum calculation for later duplicates check"); 
idCVar idImageManager::image_mipmapMode("image_mipmapMode", "2", CVAR_RENDERER | CVAR_ARCHIVE, "Mipmap generation mode: 0 - software, 1 - GL 1.4, 2 - GL 3.0"); 
// do this with a pointer, in case we want to make the actual manager
// a private virtual subclass
idImageManager	imageManager;
idImageManager	*globalImages = &imageManager;


enum IMAGE_CLASSIFICATION {
	IC_NPC,
	IC_WEAPON,
	IC_MONSTER,
	IC_MODELGEOMETRY,
	IC_ITEMS,
	IC_MODELSOTHER,
	IC_GUIS,
	IC_WORLDGEOMETRY,
	IC_OTHER,
	IC_COUNT
};

struct imageClassificate_t {
	const char *rootPath;
	const char *desc;
	int type;
	int maxWidth;
	int maxHeight;
};

typedef idList< int > intList;

const imageClassificate_t IC_Info[] = {
	{ "models/characters", "Characters", IC_NPC, 512, 512 },
	{ "models/weapons", "Weapons", IC_WEAPON, 512, 512 },
	{ "models/monsters", "Monsters", IC_MONSTER, 512, 512 },
	{ "models/mapobjects", "Model Geometry", IC_MODELGEOMETRY, 512, 512 },
	{ "models/items", "Items", IC_ITEMS, 512, 512 },
	{ "models", "Other model textures", IC_MODELSOTHER, 512, 512 },
	{ "guis/assets", "Guis", IC_GUIS, 256, 256 },
	{ "textures", "World Geometry", IC_WORLDGEOMETRY, 256, 256 },
	{ "", "Other", IC_OTHER, 256, 256 }
};


static int ClassifyImage( const char *name ) {
	const idStr str = name;

	for ( int i = 0; i < IC_COUNT; i++ ) {
		if ( str.Find( IC_Info[i].rootPath, false ) == 0 ) {
			return IC_Info[i].type;
		}
	}
	return IC_OTHER;
}

/*
================
R_RampImage

Creates a 0-255 ramp image
================
*/
static void R_RampImage( idImage *image ) {
	byte	data[256][4];

	for ( int x = 0 ; x < 256 ; x++ ) {
		data[x][0] = 
		data[x][1] = 
		data[x][2] = 
		data[x][3] = x;			
	}

	image->GenerateImage( (byte *)data, 256, 1, 
		TF_NEAREST, false, TR_CLAMP, TD_HIGH_QUALITY );
}

/*
================
R_SpecularTableImage

Creates a ramp that matches our fudged specular calculation
================
*/
static void R_SpecularTableImage( idImage *image ) {
	byte	data[256][4];
	float	f;
	int		b;

	for ( int x = 0 ; x < 256 ; x++ ) {
		f = x / 255.0f;
#if 0
		f = pow(f, 16);
#else
		// this is the behavior of the hacked up fragment programs that
		// can't really do a power function
		f = ( f - 0.75) * 4.0f;
		if ( f < 0.0f ) {
			f = 0.0f;
		}
		f = f * f;
#endif
		b = (byte)(f * 255.0f);

		data[x][0] = 
		data[x][1] = 
		data[x][2] = 
		data[x][3] = b;
	}

	image->GenerateImage( (byte *)data, 256, 1, 
		TF_LINEAR, false, TR_CLAMP, TD_HIGH_QUALITY );
}


/*
================
R_Specular2DTableImage

Create a 2D table that calculates ( reflection dot , specularity )
================
*/
static void R_Specular2DTableImage( idImage *image ) {
	byte	data[256][256][4];
	float	f;
	int		b;

	memset( data, 0, sizeof( data ) );
	for ( int x = 0 ; x < 256 ; x++ ) {
		f = x / 255.0f;
		for ( int y = 0; y < 256; y++ ) {
			b = (byte)( pow( f, y ) * 255.0f );
			if ( b == 0 ) {
				// as soon as b equals zero all remaining values in this column are going to be zero
				// we early out to avoid pow() underflows
				break;
			}

			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = 
			data[y][x][3] = b;
		}
	}

	image->GenerateImage( (byte *)data, 256, 256, TF_LINEAR, false, TR_CLAMP, TD_HIGH_QUALITY );
}


/*
================
R_AlphaRampImage

Creates a 0-255 ramp image
================
*/
#if 0
static void R_AlphaRampImage( idImage *image ) {
	byte	data[256][4];

	for ( int x = 0 ; x < 256 ; x++ ) {
		data[x][0] = 
		data[x][1] = 
		data[x][2] = 255;
		data[x][3] = x;			
	}

	image->GenerateImage( (byte *)data, 256, 1, 
		TF_NEAREST, false, TR_CLAMP, TD_HIGH_QUALITY );
}
#endif

/*
==================
R_CreateDefaultImage

the default image will be grey with a white box outline
to allow you to see the mapping coordinates on a surface
==================
*/
void idImage::MakeDefault() {
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	if ( com_developer.GetBool() ) {
		// grey center
		for ( int y = 0 ; y < DEFAULT_SIZE ; y++ ) {
			for ( int x = 0 ; x < DEFAULT_SIZE ; x++ ) {
				data[y][x][0] = 32;
				data[y][x][1] = 32;
				data[y][x][2] = 32;
				data[y][x][3] = 255;
			}
		}

		// white border
		for ( int x = 0 ; x < DEFAULT_SIZE ; x++ ) {
			data[0][x][0] =
				data[0][x][1] =
				data[0][x][2] =
				data[0][x][3] = 255;

			data[x][0][0] =
				data[x][0][1] =
				data[x][0][2] =
				data[x][0][3] = 255;

			data[DEFAULT_SIZE-1][x][0] =
				data[DEFAULT_SIZE-1][x][1] =
				data[DEFAULT_SIZE-1][x][2] =
				data[DEFAULT_SIZE-1][x][3] = 255;

			data[x][DEFAULT_SIZE-1][0] =
				data[x][DEFAULT_SIZE-1][1] =
				data[x][DEFAULT_SIZE-1][2] =
				data[x][DEFAULT_SIZE-1][3] = 255;
		}
	} else {
		for ( int y = 0 ; y < DEFAULT_SIZE ; y++ ) {
			for ( int x = 0 ; x < DEFAULT_SIZE ; x++ ) {
				data[y][x][0] = 0;
				data[y][x][1] = 0;
				data[y][x][2] = 0;
				data[y][x][3] = 0;
			}
		}
	}

	GenerateImage( (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, 
		TF_DEFAULT, true, TR_REPEAT, TD_DEFAULT );

	defaulted = true;
}

static void R_DefaultImage( idImage *image ) {
	image->MakeDefault();
}

static void R_WhiteImage( idImage *image ) {
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	// solid white texture
	memset( data, 255, sizeof( data ) );
	image->GenerateImage( (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, 
		TF_DEFAULT, false, TR_REPEAT, TD_DEFAULT );
}

static void R_BlackImage( idImage *image ) {
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	// solid black texture
	memset( data, 0, sizeof( data ) );
	image->GenerateImage( (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, 
		TF_DEFAULT, false, TR_REPEAT, TD_DEFAULT );
}

// the size determines how far away from the edge the blocks start fading
static void R_BorderClampImage( idImage *image ) {
	byte	data[BORDER_CLAMP_SIZE][BORDER_CLAMP_SIZE][4];

	// solid white texture with a single pixel black border
	memset( data, 255, sizeof( data ) );
	for ( int i = 0 ; i < BORDER_CLAMP_SIZE ; i++ ) {
		data[i][0][0] = 
		data[i][0][1] = 
		data[i][0][2] = 
		data[i][0][3] = 

		data[i][BORDER_CLAMP_SIZE-1][0] = 
		data[i][BORDER_CLAMP_SIZE-1][1] = 
		data[i][BORDER_CLAMP_SIZE-1][2] = 
		data[i][BORDER_CLAMP_SIZE-1][3] = 

		data[0][i][0] = 
		data[0][i][1] = 
		data[0][i][2] = 
		data[0][i][3] = 

		data[BORDER_CLAMP_SIZE-1][i][0] = 
		data[BORDER_CLAMP_SIZE-1][i][1] = 
		data[BORDER_CLAMP_SIZE-1][i][2] = 
		data[BORDER_CLAMP_SIZE-1][i][3] = 0;
	}

	image->GenerateImage( (byte *)data, BORDER_CLAMP_SIZE, BORDER_CLAMP_SIZE, 
		TF_LINEAR /* TF_NEAREST */, false, TR_CLAMP_TO_BORDER, TD_DEFAULT );

	if ( !glConfig.isInitialized ) {
		// can't call qglTexParameterfv yet
		return;
	}

	// explicit zero border
	float	color[4];
	color[0] = color[1] = color[2] = color[3] = 0;
	qglTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color );
}

static void R_RGBA8Image( idImage *image ) {
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	memset( data, 0, sizeof( data ) );
	data[0][0][0] = 16;
	data[0][0][1] = 32;
	data[0][0][2] = 48;
	data[0][0][3] = 96;

	image->GenerateImage( (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, 
		TF_DEFAULT, false, TR_REPEAT, TD_HIGH_QUALITY );
}

#if 0
static void R_RGB8Image( idImage *image ) {
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	memset( data, 0, sizeof( data ) );
	data[0][0][0] = 16;
	data[0][0][1] = 32;
	data[0][0][2] = 48;
	data[0][0][3] = 255;

	image->GenerateImage( (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, 
		TF_DEFAULT, false, TR_REPEAT, TD_HIGH_QUALITY );
}
#endif

static void R_AlphaNotchImage( idImage *image ) {
	byte	data[2][4];

	// this is used for alpha test clip planes

	data[0][0] = data[0][1] = data[0][2] = 255;
	data[0][3] = 0;
	data[1][0] = data[1][1] = data[1][2] = 255;
	data[1][3] = 255;

	image->GenerateImage( (byte *)data, 2, 1, 
		TF_NEAREST, false, TR_CLAMP, TD_HIGH_QUALITY );
}

static void R_FlatNormalImage( idImage *image ) {
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	const int red = 3;
	const int alpha = 0;

	// flat normal map for default bunp mapping
	for ( int i = 0 ; i < 4 ; i++ ) {
		data[0][i][red] = 128;
		data[0][i][1] = 128;
		data[0][i][2] = 255;
		data[0][i][alpha] = 255;
	}
	image->GenerateImage( (byte *)data, 2, 2, 
		TF_DEFAULT, true, TR_REPEAT, TD_HIGH_QUALITY );
}

static void R_AmbientNormalImage( idImage *image ) {
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	const int red = 3;
	const int alpha = 0;

	// flat normal map for default bunp mapping
	for ( int i = 0 ; i < 4 ; i++ ) {
		data[0][i][red] = (byte)(255 * tr.ambientLightVector[0]);
		data[0][i][1] = (byte)(255 * tr.ambientLightVector[1]);
		data[0][i][2] = (byte)(255 * tr.ambientLightVector[2]);
		data[0][i][alpha] = 255;
	}

	const byte	*pics[6];
	for ( int i = 0 ; i < 6 ; i++ ) {
		pics[i] = data[0][0];
	}

	// this must be a cube map for fragment programs to simply substitute for the normalization cube map
	image->GenerateCubeImage( pics, 2, TF_DEFAULT, true, TD_HIGH_QUALITY );
}

#if 0
static void CreateSquareLight( void ) {
	byte		*buffer;
	int			dx, dy;
	int			d;

	const int width = 128;
	const int height = 128;

	buffer = (byte *)R_StaticAlloc( 128 * 128 * 4 );

	for ( int x = 0 ; x < 128 ; x++ ) {
		if ( x < 32 ) {
			dx = 32 - x;
		} else if ( x > 96 ) {
			dx = x - 96;
		} else {
			dx = 0;
		}
		for ( int y = 0 ; y < 128 ; y++ ) {
			if ( y < 32 ) {
				dy = 32 - y;
			} else if ( y > 96 ) {
				dy = y - 96;
			} else {
				dy = 0;
			}
			d = (byte)idMath::Sqrt( dx * dx + dy * dy );
			if ( d > 32 ) {
				d = 32;
			}
			d = 255 - d * 8;
			if ( d < 0 ) {
				d = 0;
			}
			buffer[(y*128+x)*4+0] =
			buffer[(y*128+x)*4+1] =
			buffer[(y*128+x)*4+2] = d;
			buffer[(y*128+x)*4+3] = 255;
		}
	}

	R_WriteTGA( "lights/squarelight.tga", buffer, width, height );

	R_StaticFree( buffer );
}

static void CreateFlashOff( void ) {
	byte		*buffer;
	int			d;

	const int width = 256;
	const int height = 4;

	buffer = (byte *)R_StaticAlloc( width * height * 4 );

	for ( int x = 0 ; x < width ; x++ ) {
		for ( int y = 0 ; y < height ; y++ ) {
			d = 255 - ( x * 256 / width );
			buffer[(y*width+x)*4+0] =
			buffer[(y*width+x)*4+1] =
			buffer[(y*width+x)*4+2] = d;
			buffer[(y*width+x)*4+3] = 255;
		}
	}

	R_WriteTGA( "lights/flashoff.tga", buffer, width, height );

	R_StaticFree( buffer );
}
#endif

/*
===============
CreatePitFogImage
===============
*/
void CreatePitFogImage( void ) {
	byte	data[16][16][4];
	int		a;

	memset( data, 0, sizeof( data ) );

	for ( int i = 0 ; i < 16 ; i++ ) {
		a = i * ( 255 / 15 );
		if ( a > 255 ) {
			a = 255;
		}

		for ( int j = 0 ; j < 16 ; j++ ) {
			data[j][i][0] =
			data[j][i][1] =
			data[j][i][2] = 255;
			data[j][i][3] = a;
		}
	}

	R_WriteTGA( "shapes/pitFalloff.tga", data[0][0], 16, 16 );
}


/*
===============
CreatealphaSquareImage
===============
*/
void CreatealphaSquareImage( void ) {
	byte	data[16][16][4];
	int	a;

	for ( int i = 0 ; i < 16 ; i++ ) {
		for ( int j = 0 ; j < 16 ; j++ ) {
			if ( i == 0 || i == 15 || j == 0 || j == 15 ) {
				a = 0;
			} else {
				a = 255;
			}
			data[j][i][0] =
			data[j][i][1] =
			data[j][i][2] = 255;
			data[j][i][3] = a;
		}
	}

	R_WriteTGA( "shapes/alphaSquare.tga", data[0][0], 16, 16 );
}


/*** NORMALIZATION CUBE MAP CONSTRUCTION ***/

/* Given a cube map face index, cube map size, and integer 2D face position,
 * return the cooresponding normalized vector.
 */
static void getCubeVector(int i, int cubesize, int x, int y, float *vector) {

	const float s = (float)((x + 0.5f) / (unsigned int)cubesize);
	const float t = (float)((y + 0.5f) / (unsigned int)cubesize);
	const float sc = (s * 2.0f) - 1.0f;
	const float tc = (t * 2.0f) - 1.0f;

	switch (i) {
	case 0:
		vector[0] = 1.0f;
		vector[1] = -tc;
		vector[2] = -sc;
		break;
	case 1:
		vector[0] = -1.0f;
		vector[1] = -tc;
		vector[2] = sc;
		break;
	case 2:
		vector[0] = sc;
		vector[1] = 1.0f;
		vector[2] = tc;
		break;
	case 3:
		vector[0] = sc;
		vector[1] = -1.0f;
		vector[2] = -tc;
		break;
	case 4:
		vector[0] = sc;
		vector[1] = -tc;
		vector[2] = 1.0f;
		break;
	case 5:
		vector[0] = -sc;
		vector[1] = -tc;
		vector[2] = -1.0f;
		break;
    default:
        common->Error("getCubeVector: invalid cube map face index");
        return;
	}

	const float mag = idMath::InvSqrt(vector[0]*vector[0] + vector[1]*vector[1] + vector[2]*vector[2]);
	vector[0] *= mag;
	vector[1] *= mag;
	vector[2] *= mag;
}


/* Initialize a cube map texture object that generates RGB values
 * that when expanded to a [-1,1] range in the register combiners
 * form a normalized vector matching the per-pixel vector used to
 * access the cube map.
 */
static void makeNormalizeVectorCubeMap( idImage *image ) {
	float vector[3] = { };
	byte	*pixels[6];

	const int size = NORMAL_MAP_SIZE;

	pixels[0] = (GLubyte*) Mem_Alloc(size*size*4*6);

	for ( int i = 0; i < 6; i++ ) {
		pixels[i] = pixels[0] + i*size*size*4;
		for ( int y = 0; y < size; y++ ) {
		  for ( int x = 0; x < size; x++ ) {
			getCubeVector(i, size, x, y, vector);
			pixels[i][4*(y*size+x) + 0] = (byte)(128 + 127*vector[0]);
			pixels[i][4*(y*size+x) + 1] = (byte)(128 + 127*vector[1]);
			pixels[i][4*(y*size+x) + 2] = (byte)(128 + 127*vector[2]);
			pixels[i][4*(y*size+x) + 3] = 255;
		  }
		}
	}

	image->GenerateCubeImage( (const byte **)pixels, size,
						   TF_LINEAR, false, TD_HIGH_QUALITY ); 

	Mem_Free(pixels[0]);
}


/*
================
R_CreateNoFalloffImage

This is a solid white texture that is zero clamped.
================
*/
static void R_CreateNoFalloffImage( idImage *image ) {
	byte	data[16][FALLOFF_TEXTURE_SIZE][4];

	memset( data, 0, sizeof( data ) );
	for ( int x = 1 ; x < FALLOFF_TEXTURE_SIZE-1 ; x++ ) {
		for ( int y = 1 ; y < 15 ; y++ ) {
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = 255;
		}
	}
	image->GenerateImage( (byte *)data, FALLOFF_TEXTURE_SIZE, 16,
		TF_DEFAULT, false, TR_CLAMP_TO_ZERO, TD_HIGH_QUALITY );
}


/*
================
R_FogImage

We calculate distance correctly in two planes, but the
third will still be projection based
================
*/
void R_FogImage( idImage *image ) {
	byte	data[FOG_SIZE][FOG_SIZE][4];
	int		b;
	float	d;

	float	step[256];
	float	remaining = 1.0f;

	for ( int i = 0 ; i < 256 ; i++ ) {
		step[i] = remaining;
		remaining *= 0.982f;
	}

	for ( int x = 0 ; x < FOG_SIZE ; x++ ) {
		for ( int y = 0 ; y < FOG_SIZE ; y++ ) {
			d = idMath::Sqrt((x-FOG_SIZE/2)*(x-FOG_SIZE/2)+(y-FOG_SIZE/2)*(y-FOG_SIZE/2))/((FOG_SIZE/2)-1.0f);
			b = (byte)(d * 255);

			if ( b <= 0 ) {
				b = 0;
			} else if ( b > 255 ) {
				b = 255;
			}

			b = (byte)(255 * ( 1.0f - step[b] ));
			if ( x == 0 || x == FOG_SIZE-1 || y == 0 || y == FOG_SIZE-1 ) {
				b = 255;		// avoid clamping issues
			}

			data[y][x][0] =
			data[y][x][1] =
			data[y][x][2] = 255;
			data[y][x][3] = b;
		}
	}

	image->GenerateImage( (byte *)data, FOG_SIZE, FOG_SIZE, 
		TF_LINEAR, false, TR_CLAMP, TD_HIGH_QUALITY );
}


/*
================
FogFraction

Height values below zero are inside the fog volume
================
*/
static float FogFraction( float viewHeight, float targetHeight ) {
	float	total = idMath::Fabs( targetHeight - viewHeight );

	// only ranges that cross the ramp range are special
	if ( targetHeight > 0 && viewHeight > 0 ) {
		return 0.0f;
	} else if ( targetHeight < -RAMP_RANGE && viewHeight < -RAMP_RANGE ) {
		return 1.0f;
	}

	float rampSlope = 1.0f / RAMP_RANGE;

	if ( !total ) {
		return -viewHeight * rampSlope;
	}

	float above;
	if ( targetHeight > 0.0f ) {
		above = targetHeight;
	} else if ( viewHeight > 0.0f ) {
		above = viewHeight;
	} else {
		above = 0.0f;
	}

	float rampTop, rampBottom;

	if ( viewHeight > targetHeight ) {
		rampTop = viewHeight;
		rampBottom = targetHeight;
	} else {
		rampTop = targetHeight;
		rampBottom = viewHeight;
	}

	if ( rampTop > 0.0f ) {
		rampTop = 0.0f;
	}
	if ( rampBottom < -RAMP_RANGE ) {
		rampBottom = -RAMP_RANGE;
	}

	float ramp = ( 1.0f - ( rampTop * rampSlope + rampBottom * rampSlope ) * -0.5f ) * ( rampTop - rampBottom );

	float frac = ( total - above - ramp ) / total;

	// after it gets moderately deep, always use full value
	float deepest = viewHeight < targetHeight ? viewHeight : targetHeight;

	float deepFrac = deepest / DEEP_RANGE;

	if ( deepFrac >= 1.0f ) {
		return 1.0f;
	}

	frac = frac * ( 1.0f - deepFrac ) + deepFrac;

	return frac;
}

/*
================
R_FogEnterImage

Modulate the fog alpha density based on the distance of the
start and end points to the terminator plane
================
*/
void R_FogEnterImage( idImage *image ) {
	byte	data[FOG_ENTER_SIZE][FOG_ENTER_SIZE][4];
	int		b;
	float	d;

	for ( int x = 0 ; x < FOG_ENTER_SIZE ; x++ ) {
		for ( int y = 0 ; y < FOG_ENTER_SIZE ; y++ ) {
			d = FogFraction( x - (FOG_ENTER_SIZE / 2), y - (FOG_ENTER_SIZE / 2) );
			b = (byte)(d * 255.0f);

			if ( b < 1 ) { // Rounding issues
				b = 0;
			} else if ( b > 254 ) { // Rounding issues
				b = 255;
			}

			data[y][x][0] =
			data[y][x][1] =
			data[y][x][2] = 255;
			data[y][x][3] = b;
		}
	}

	// if mipmapped, acutely viewed surfaces fade wrong
	image->GenerateImage( (byte *)data, FOG_ENTER_SIZE, FOG_ENTER_SIZE, 
		TF_LINEAR, false, TR_CLAMP, TD_HIGH_QUALITY );
}


/*
================
R_QuadraticImage

================
*/
void R_QuadraticImage( idImage *image ) {
	byte	data[QUADRATIC_HEIGHT][QUADRATIC_WIDTH][4];
	int		b;
	float	d;

	for ( int x = 0 ; x<QUADRATIC_WIDTH ; x++ ) {
		for ( int y = 0 ; y<QUADRATIC_HEIGHT ; y++ ) {

			d = x - (QUADRATIC_WIDTH/2 - 0.5f);
			d = idMath::Fabs( d );
			d -= 0.5f;
			d /= QUADRATIC_WIDTH/2;
		
			d = 1.0f - d;
			d = d * d;

			b = (byte)(d * 255);
			if ( b <= 0 ) {
				b = 0;
			} else if ( b > 255 ) {
				b = 255;
			}

			data[y][x][0] =
			data[y][x][1] =
			data[y][x][2] = b;
			data[y][x][3] = 255;
		}
	}

	image->GenerateImage( (byte *)data, QUADRATIC_WIDTH, QUADRATIC_HEIGHT, 
		TF_DEFAULT, false, TR_CLAMP, TD_HIGH_QUALITY );
}

//=====================================================================

typedef struct {
	const char *name;
	int	minimize, maximize;
} filterName_t;

static filterName_t textureFilters[] = {
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR},
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST}
};

/*
===============
ChangeTextureFilter

This resets filtering on all loaded images
New images will automatically pick up the current values.
===============
*/
void idImageManager::ChangeTextureFilter( void ) {
	int		i;
	idImage	*glt;

	// if these are changed dynamically, it will force another ChangeTextureFilter
	image_filter.ClearModified();
	image_anisotropy.ClearModified();
	image_lodbias.ClearModified();

	const char *string = image_filter.GetString();
	for ( i = 0; i < 6; i++ ) {
		if ( !idStr::Icmp( textureFilters[i].name, string ) ) {
			break;
		}
	}

	if ( i == 6 ) {
		common->Warning( "bad r_textureFilter: '%s'", string);
		// default to LINEAR_MIPMAP_NEAREST
		i = 0;
	}

	// set the values for future images
	textureMinFilter = textureFilters[i].minimize;
	textureMaxFilter = textureFilters[i].maximize;
	textureAnisotropy = image_anisotropy.GetFloat();
	if ( textureAnisotropy < 1 ) {
		textureAnisotropy = 1;
	} else if ( textureAnisotropy > glConfig.maxTextureAnisotropy ) {
		textureAnisotropy = glConfig.maxTextureAnisotropy;
	}
	textureLODBias = image_lodbias.GetFloat();

	// change all the existing mipmap texture objects with default filtering
	unsigned int texEnum;
	for ( i = 0 ; i < images.Num() ; i++ ) {
		glt = images[ i ];

		switch( glt->type ) {
		case TT_2D:
			texEnum = GL_TEXTURE_2D;
			break;
		case TT_CUBIC:
			texEnum = GL_TEXTURE_CUBE_MAP;
			break;
		default:
			texEnum = GL_TEXTURE_2D;
		}

		// make sure we don't start a background load
		if ( glt->texnum == idImage::TEXTURE_NOT_LOADED ) {
			continue;
		} else {
			glt->Bind();
			if ( glt->filter == TF_DEFAULT ) {
				qglTexParameterf(texEnum, GL_TEXTURE_MIN_FILTER, globalImages->textureMinFilter );
				qglTexParameterf(texEnum, GL_TEXTURE_MAG_FILTER, globalImages->textureMaxFilter );
			}
			if ( glConfig.anisotropicAvailable ) {
				qglTexParameterf(texEnum, GL_TEXTURE_MAX_ANISOTROPY_EXT, globalImages->textureAnisotropy );
			}	
			if ( glConfig.textureLODBiasAvailable ) {
				qglTexParameterf(texEnum, GL_TEXTURE_LOD_BIAS_EXT, globalImages->textureLODBias );
			}
		}
	}
}

/*
===============
idImage::Reload
===============
*/
void idImage::Reload( bool checkPrecompressed, bool force ) {

	// always regenerate functional images
	if ( generatorFunction ) {
		generatorFunction( this );
		common->DPrintf( "regenerating %s.\n", imgName.c_str() );
		return;
	} else if ( !force ) { // check file times
		ID_TIME_T	current;

		if ( cubeFiles != CF_2D ) {
			R_LoadCubeImages( imgName, cubeFiles, NULL, NULL, &current );
		} else { // get the current values
			R_LoadImageProgram( imgName, NULL, NULL, NULL, &current );
		}

		if ( current <= timestamp ) {
			return;
		}
	}

	common->DPrintf( "reloading %s.\n", imgName.c_str() );

	PurgeImage();

	// force no precompressed image check, which will cause it to be reloaded
	// from source, and another precompressed file generated.
	// Load is from the front end, so the back end must be synced
	ActuallyLoadImage( checkPrecompressed, false );
}

/*
===============
R_ReloadImages_f

Regenerate all images that came directly from files that have changed, so
any saved changes will show up in place.

New r_texturesize/r_texturedepth variables will take effect on reload

reloadImages <all>
===============
*/
void R_ReloadImages_f( const idCmdArgs &args ) {
	idImage	*image;

	// FIXME - this probably isn't necessary... // Serp - this is a comment from the gpl release, check if it's really not needed
	globalImages->ChangeTextureFilter();

	bool	normalsOnly = false, force = false;
	bool	checkPrecompressed = false;		// if we are doing this as a vid_restart, look for precompressed like normal

	if ( args.Argc() == 2 ) {
		if ( !idStr::Icmp( args.Argv(1), "all" ) ) {
			force = true;
		}
		else if ( !idStr::Icmp( args.Argv( 1 ), "bump" ) ) {
			force = true;
			normalsOnly = true;
		}
		else if ( !idStr::Icmp( args.Argv( 1 ), "reload" ) ) {
			force = true;
			checkPrecompressed = true;
		} else {
			common->Printf( "USAGE: reloadImages <all>\n" );
			return;
		}
	}

	for ( int i = 0 ; i < globalImages->images.Num() ; i++ ) {
		image = globalImages->images[ i ];
		if ( image->depth != TD_BUMP && normalsOnly )
			continue;
		image->Reload( checkPrecompressed, force );
	}

	if ( game ) {
		game->OnReloadImages();
	}
}

typedef struct {
	idImage	*image;
	int		size;
} sortedImage_t;

/*
=======================
R_QsortImageSizes

=======================
*/
static int R_QsortImageSizes( const void *a, const void *b ) {

	const sortedImage_t	*ea = (sortedImage_t *)a;
	const sortedImage_t	*eb = (sortedImage_t *)b;

	if ( ea->size > eb->size ) {
		return -1;
	} else if ( ea->size < eb->size ) {
		return 1;
	} else {
		return idStr::Icmp( ea->image->imgName, eb->image->imgName );
	}
}

/*
===============
R_ListImages_f
===============
*/
void R_ListImages_f( const idCmdArgs &args ) {
	int		i, j, partialSize;
	idImage	*image;
	int		totalSize;
	int		count = 0;
	int		matchTag = 0;
	bool	uncompressedOnly = false;
	bool	unloaded = false;
	bool	partial = false;
	bool	cached = false;
	bool	uncached = false;
	bool	failed = false;
	bool	touched = false;
	bool	sorted = false;
	bool	duplicated = false;
	bool	byClassification = false;
	bool	overSized = false;

	if ( args.Argc() == 1 ) {

	} else if ( args.Argc() == 2 ) {
		if ( idStr::Icmp( args.Argv( 1 ), "uncompressed" ) == 0 ) {
			uncompressedOnly = true;
		} else if ( idStr::Icmp( args.Argv( 1 ), "sorted" ) == 0 ) {
			sorted = true;
		} else if ( idStr::Icmp( args.Argv( 1 ), "partial" ) == 0 ) {
			partial = true;
		} else if ( idStr::Icmp( args.Argv( 1 ), "unloaded" ) == 0 ) {
			unloaded = true;
		} else if ( idStr::Icmp( args.Argv( 1 ), "cached" ) == 0 ) {
			cached = true;
		} else if ( idStr::Icmp( args.Argv( 1 ), "uncached" ) == 0 ) {
			uncached = true;
		} else if ( idStr::Icmp( args.Argv( 1 ), "tagged" ) == 0 ) {
			matchTag = 1;
		} else if ( idStr::Icmp( args.Argv( 1 ), "duplicated" ) == 0 ) {
			duplicated = true;
		} else if ( idStr::Icmp( args.Argv( 1 ), "touched" ) == 0 ) {
			touched = true;
		} else if ( idStr::Icmp( args.Argv( 1 ), "classify" ) == 0 ) {
			byClassification = true;
			sorted = true;
		} else if ( idStr::Icmp( args.Argv( 1 ), "oversized" ) == 0 ) {
			byClassification = true;
			sorted = true;
			overSized = true;
		} else {
			failed = true;
		}
	} else {
		failed = true;
	}

	if ( failed ) {
		common->Printf( "usage: listImages [ sorted | partial | unloaded | cached | uncached | tagged | duplicated | touched | classify | showOverSized ]\n" );
		return;
	}

	const char *header = "       -w-- -h-- filt -fmt-- wrap  size --name-------\n";
	common->Printf( "\n%s", header );

	totalSize = 0;

	sortedImage_t	*sortedArray = (sortedImage_t *)alloca( sizeof( sortedImage_t ) * globalImages->images.Num() );

	for ( i = 0 ; i < globalImages->images.Num() ; i++ ) {
		image = globalImages->images[ i ];

		if ( uncompressedOnly ) {
			if ( image->internalFormat >= GL_COMPRESSED_RGB_S3TC_DXT1_EXT && image->internalFormat <= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ) {
				continue;
			}
		}

		if ( matchTag && image->classification != matchTag ) {
			continue;
		}
		if ( unloaded && image->texnum != idImage::TEXTURE_NOT_LOADED ) {
			continue;
		}
		if ( partial && !image->isPartialImage ) {
			continue;
		}
		if ( cached && ( !image->partialImage || image->texnum == idImage::TEXTURE_NOT_LOADED ) ) {
			continue;
		}
		if ( uncached && ( !image->partialImage || image->texnum != idImage::TEXTURE_NOT_LOADED ) ) {
			continue;
		}

		// only print duplicates (from mismatched wrap / clamp, etc)
		if ( duplicated ) {
			for ( j = i+1 ; j < globalImages->images.Num() ; j++ ) {
				if ( idStr::Icmp( image->imgName, globalImages->images[ j ]->imgName ) == 0 ) {
					break;
				}
			}
			if ( j == globalImages->images.Num() ) {
				continue;
			}
		}

		// "listimages touched" will list only images bound since the last "listimages touched" call
		if ( touched ) {
			if ( image->bindCount == 0 ) {
				continue;
			}
			image->bindCount = 0;
		}

		if ( sorted ) {
			sortedArray[count].image = image;
			sortedArray[count].size = image->StorageSize();
		} else {
			common->Printf( "%4i:",	i );
			image->Print();
		}
		totalSize += image->StorageSize();
		count++;
	}

	if ( sorted ) {
		qsort( sortedArray, count, sizeof( sortedImage_t ), R_QsortImageSizes );
		partialSize = 0;
		for ( i = 0 ; i < count ; i++ ) {
			common->Printf( "%4i:",	i );
			sortedArray[i].image->Print();
			partialSize += sortedArray[i].image->StorageSize();
			if ( ( (i+1) % 10 ) == 0 ) {
				common->Printf( "-------- %5.1f of %5.1f megs --------\n", 
					partialSize / (1024*1024.0), totalSize / (1024*1024.0) );
			}
		}
	}

	common->Printf( "%s", header );
	common->Printf( " %i images (%i total)\n", count, globalImages->images.Num() );
	common->Printf( " %5.1f total megabytes of images\n\n\n", totalSize / (1024*1024.0) );

	if ( byClassification ) {
		idList< int > classifications[IC_COUNT];

		for ( i = 0 ; i < count ; i++ ) {
			int cl = ClassifyImage( sortedArray[i].image->imgName );
			classifications[ cl ].Append( i );
		}

		for ( i = 0; i < IC_COUNT; i++ ) {
			partialSize = 0;
			idList< int > overSizedList;
			for ( j = 0; j < classifications[ i ].Num(); j++ ) {
				partialSize += sortedArray[ classifications[ i ][ j ] ].image->StorageSize();
				if ( overSized ) {
					if ( sortedArray[ classifications[ i ][ j ] ].image->uploadWidth > IC_Info[i].maxWidth && sortedArray[ classifications[ i ][ j ] ].image->uploadHeight > IC_Info[i].maxHeight ) {
						overSizedList.Append( classifications[ i ][ j ] );
					}
				}
			}
			common->Printf ( " Classification %s contains %i images using %5.1f megabytes\n", IC_Info[i].desc, classifications[i].Num(), partialSize / ( 1024*1024.0 ) );
			if ( overSized && overSizedList.Num() ) {
				common->Printf( "  The following images may be oversized\n" );
				for ( j = 0; j < overSizedList.Num(); j++ ) {
					common->Printf( "    " );
					sortedArray[ overSizedList[ j ] ].image->Print();
					common->Printf( "\n" );
				}
			}
		}
	}

}

/*
==================
SetNormalPalette

Create a 256 color palette to be used by compressed normal maps
==================
*/
void idImageManager::SetNormalPalette( void ) {
	idVec3	v;
	int j;
	//byte temptable[768];
	byte	*temptable = compressedPalette;
	int		compressedToOriginal[16];
	float	t, f, y;

	// make an ad-hoc separable compression mapping scheme
	for ( int i = 0 ; i < 8 ; i++ ) {
		f = ( i + 1 ) / 8.5f;
		y =  1.0f - idMath::Sqrt( 1.0f - f * f );

		compressedToOriginal[7-i] = 127 - (int)( y * 127 + 0.5f );
		compressedToOriginal[8+i] = 128 + (int)( y * 127 + 0.5f );
	}

	for ( int i = 0 ; i < 256 ; i++ ) {
		if ( i <= compressedToOriginal[0] ) {
			originalToCompressed[i] = 0;
		} else if ( i >= compressedToOriginal[15] ) {
			originalToCompressed[i] = 15;
		} else {
			for ( j = 0 ; j < 14 ; j++ ) {
				if ( i <= compressedToOriginal[j+1] ) {
					break;
				}
			}
			if ( i - compressedToOriginal[j] < compressedToOriginal[j+1] - i ) {
				originalToCompressed[i] = j;
			} else {
				originalToCompressed[i] = j + 1;
			}
		}
	}

#if 0
	for ( i = 0; i < 16; i++ ) {
		for ( j = 0 ; j < 16 ; j++ ) {

			v[0] = ( i - 7.5 ) / 8;
			v[1] = ( j - 7.5 ) / 8;

			t = 1.0 - ( v[0]*v[0] + v[1]*v[1] );
			if ( t < 0 ) {
				t = 0;
			}
			v[2] = idMath::Sqrt( t );

			temptable[(i*16+j)*3+0] = 128 + floor( 127 * v[0] + 0.5 );
			temptable[(i*16+j)*3+1] = 128 + floor( 127 * v[1] );
			temptable[(i*16+j)*3+2] = 128 + floor( 127 * v[2] );
		}
	}
#else
	for ( int i = 0; i < 16; i++ ) {
		for ( j = 0 ; j < 16 ; j++ ) {

			v[0] = ( compressedToOriginal[i] - 127.5 ) / 128;
			v[1] = ( compressedToOriginal[j] - 127.5 ) / 128;

			t = 1.0 - ( v[0]*v[0] + v[1]*v[1] );
			if ( t < 0 ) {
				t = 0;
			}
			v[2] = idMath::Sqrt( t );

			temptable[(i*16+j)*3+0] = (byte)(128 + floor( 127 * v[0] + 0.5 ));
			temptable[(i*16+j)*3+1] = (byte)(128 + floor( 127 * v[1] ));
			temptable[(i*16+j)*3+2] = (byte)(128 + floor( 127 * v[2] ));
		}
	}
#endif

	// color 255 will be the "nullnormal" color for no reflection
	temptable[255*3+0] =
	temptable[255*3+1] =
	temptable[255*3+2] = 128;
}

/*
==============
AllocImage

Allocates an idImage, adds it to the list,
copies the name, and adds it to the hash chain.
==============
*/
idImage *idImageManager::AllocImage( const char *name ) {

	if (strlen(name) >= MAX_IMAGE_NAME || strlen(name) < MIN_IMAGE_NAME ) {
		const char* warnp  = (strlen(name) >= MAX_IMAGE_NAME) ? "long" : "short";
		common->Warning("Image name \"%s\" is too %s", name, warnp);
	}

	idImage *image = new idImage;

	images.Append( image );
	//common->Printf("AllocImage added image '%s'\n",name);

	const int hash = idStr( name ).FileNameHash();

	image->hashNext = imageHashTable[hash];
	imageHashTable[hash] = image;

	image->imgName = name;

	return image;
}

/*
==================
ImageFromFunction

Images that are procedurally generated are always specified
with a callback which must work at any time, allowing the OpenGL
system to be completely regenerated if needed.
==================
*/
idImage *idImageManager::ImageFromFunction( const char *_name, void (*generatorFunction)( idImage *image ) ) {

	if ( !_name ) {
		common->FatalError( "idImageManager::ImageFromFunction: NULL name" );
	}

	idImage	*image;

	// strip any .tga file extensions from anywhere in the _name
	idStr name = _name;
	name.Remove( ".tga" );
	name.BackSlashesToSlashes();

	// see if the image already exists
	int	hash = name.FileNameHash();
	for ( image = imageHashTable[hash] ; image; image = image->hashNext ) {
		if ( name.Icmp( image->imgName ) == 0 ) {
			if ( image->generatorFunction != generatorFunction ) {
				common->Warning( "Reused image %s with mixed generators", name.c_str() );
			}
			return image;
		}
	}

	// create the image and issue the callback
	image = AllocImage( name );

	image->generatorFunction = generatorFunction;

	// check for precompressed, load is from the front end
	if ( image_preload.GetBool() ) {
		image->referencedOutsideLevelLoad = true;
		image->ActuallyLoadImage( true, false );
	}

	return image;
}

/* ~ss
==================
RendertargetImage

An image suitable for a render target. No mipmaps, no downsizing. Calling code specifies the internal format. 
Can be called with an existing image name to resize an image or change its format
==================

idImage *idImageManager::RendertargetImage( const char* name, int width, int height, GLuint internalFormat, GLuint pixelFormat, GLuint pixelType )
{
	idImage* img = GetImage(name);

	if ( !img )
	{
		img = AllocImage( name );
		img->generatorFunction = R_RendertargetImage;
		img->referencedOutsideLevelLoad = true;
	}

	img->uploadWidth = width;
	img->uploadHeight = height;
	img->internalFormat = internalFormat;
	img->pixelDataFormat[0] = pixelFormat;
	img->pixelDataFormat[1] = pixelType;
	
	// NB there's no difference between ActuallyLoadImage() and Reload() for generated images. Both just call the generator function.
	img->ActuallyLoadImage( false, true );
	return img;
}

/*

/*
===============
ImageFromFile

Finds or loads the given image, always returning a valid image pointer.
Loading of the image may be deferred for dynamic loading.
==============
*/
idImage	*idImageManager::ImageFromFile( const char *_name, textureFilter_t filter, bool allowDownSize,
						 textureRepeat_t repeat, textureDepth_t depth, cubeFiles_t cubeMap ) {

	if ( !_name || !_name[0] || idStr::Icmp( _name, "default" ) == 0 || idStr::Icmp( _name, "_default" ) == 0 ) {
		declManager->MediaPrint( "DEFAULTED\n" );
		return globalImages->defaultImage;
	}

	idImage	*image;

	// strip any .tga file extensions from anywhere in the _name, including image program parameters
	idStr name = _name;
	name.Remove( ".tga" );
	name.BackSlashesToSlashes();

	// see if the image is already loaded, unless we are in a reloadImages call

	int hash = name.FileNameHash();
	for ( image = imageHashTable[hash]; image; image = image->hashNext ) {
		if ( name.Icmp( image->imgName ) == 0 ) {
			// the built in's, like _white and _flat always match the other options
			if ( name[0] == '_' ) {
				return image;
			}
			if ( image->cubeFiles != cubeMap ) {
				common->Error( "Image '%s' has been referenced with conflicting cube map states", _name );
			}

			if ( image->filter != filter || image->repeat != repeat ) {
				// we might want to have the system reset these parameters on every bind and share the image data
				continue;
			}

			if ( image->allowDownSize == allowDownSize && image->depth == depth ) {
				// note that it is used this level load
				image->levelLoadReferenced = true;
				if ( image->partialImage != NULL ) {
					image->partialImage->levelLoadReferenced = true;
				}
				return image;
			}

			// the same image is being requested, but with a different allowDownSize or depth
			// so pick the highest of the two and reload the old image with those parameters
			if ( !image->allowDownSize ) {
				allowDownSize = false;
			}
			if ( image->depth > depth ) {
				depth = image->depth;
			}
			if ( image->allowDownSize == allowDownSize && image->depth == depth ) {
				// the already created one is already the highest quality
				image->levelLoadReferenced = true;
				if ( image->partialImage != NULL ) {
					image->partialImage->levelLoadReferenced = true;
				}
				return image;
			}

			image->allowDownSize = allowDownSize;
			image->depth = depth;
			image->levelLoadReferenced = true;
			if ( image->partialImage != NULL ) {
				image->partialImage->levelLoadReferenced = true;
			}
			if ( image_preload.GetBool() && !insideLevelLoad ) {
				image->referencedOutsideLevelLoad = true;
				image->ActuallyLoadImage( true, false );	// check for precompressed, load is from front end
				declManager->MediaPrint( "%ix%i %s (reload for mixed references)\n", image->uploadWidth, image->uploadHeight, image->imgName.c_str() );
			}
			return image;
		}
	}

	//
	// create a new image
	//
	image = AllocImage( name );

	// HACK: to allow keep fonts from being mip'd, as new ones will be introduced with localization
	// this keeps us from having to make a material for each font tga
	if ( name.Find( "fontImage_") >= 0 
	//nbohr1more: 4358 blacklist texture paths to prevent image_downsize from making fonts, guis, and background images blurry
	|| name.Find("fonts/") >= 0
	|| name.Find("guis/assets/") >= 0
	|| name.Find("postprocess/") >= 0
	|| name.Find("_cookedMath") >= 0
	|| name.Find("_currentRender") >= 0
	|| name.Find("_currentDepth") >= 0
	|| name.Find("/consolefont") >= 0
	|| name.Find("/bigchars") >= 0
	|| name.Find("/entityGui") >= 0
	|| name.Find("video/") >= 0
	|| name.Find("fsfx") >= 0
	|| name.Find("/AFX") >= 0
	|| name.Find("_afxweight") >= 0
	|| name.Find("_bloomImage") >= 0)
		allowDownSize = false;

	image->allowDownSize = allowDownSize;
	image->repeat = repeat;
	image->depth = depth;
	image->type = TT_2D;
	image->cubeFiles = cubeMap;
	image->filter = filter;
	
	image->levelLoadReferenced = true;

	// also create a shrunken version if we are going to dynamically cache the full size image
	if ( image->ShouldImageBePartialCached() ) {
		// if we only loaded part of the file, create a new idImage for the shrunken version
		image->partialImage = new idImage;

		image->partialImage->allowDownSize = allowDownSize;
		image->partialImage->repeat = repeat;
		image->partialImage->depth = depth;
		image->partialImage->type = TT_2D;
		image->partialImage->cubeFiles = cubeMap;
		image->partialImage->filter = filter;

		image->partialImage->levelLoadReferenced = true;

		// we don't bother hooking this into the hash table for lookup, but we do add it to the manager
		// list for listImages
		globalImages->images.Append( image->partialImage );
		image->partialImage->imgName = image->imgName;
		image->partialImage->isPartialImage = true;

		// let the background file loader know that we can load
		image->precompressedFile = true;

		if ( image_preload.GetBool() && !insideLevelLoad ) {
			image->partialImage->ActuallyLoadImage( true, false );	// check for precompressed, load is from front end
			declManager->MediaPrint( "%ix%i %s\n", image->partialImage->uploadWidth, image->partialImage->uploadHeight, image->imgName.c_str() );
		} else {
			declManager->MediaPrint( "%s\n", image->imgName.c_str() );
		}
		return image;
	}

	// load it if we aren't in a level preload
	if ( image_preload.GetBool() && !insideLevelLoad ) {
		image->referencedOutsideLevelLoad = true;
		image->ActuallyLoadImage( true, false );	// check for precompressed, load is from front end
		declManager->MediaPrint( "%ix%i %s\n", image->uploadWidth, image->uploadHeight, image->imgName.c_str() );
	} else {
		declManager->MediaPrint( "%s\n", image->imgName.c_str() );
	}

	return image;
}

/*
===============
idImageManager::GetImage
===============
*/
idImage *idImageManager::GetImage( const char *_name ) const {

	if ( !_name || !_name[0] || idStr::Icmp( _name, "default" ) == 0 || idStr::Icmp( _name, "_default" ) == 0 ) {
		declManager->MediaPrint( "DEFAULTED\n" );
		return globalImages->defaultImage;
	}

	idImage	*image;

	// strip any .tga file extensions from anywhere in the _name, including image program parameters
	idStr name = _name;
	name.Remove( ".tga" );
	name.BackSlashesToSlashes();

	// look in loaded images
	int hash = name.FileNameHash();
	for ( image = imageHashTable[hash]; image; image = image->hashNext ) {
		if ( name.Icmp( image->imgName ) == 0 ) {
			return image;
		}
	}

	return NULL;
}

/*
===============
PurgeAllImages
===============
*/
void idImageManager::PurgeAllImages() {
	idImage	*image;

	for ( int i = 0; i < images.Num() ; i++ ) {
		image = images[i];
		image->PurgeImage();
	}
}

/*
===============
ReloadAllImages
===============
*/
void idImageManager::ReloadAllImages() {
	idCmdArgs args;

	// build the compressed normal map palette
	SetNormalPalette();

	args.TokenizeString( "reloadImages reload", false );
	R_ReloadImages_f( args );
}

/*
===============
R_CombineCubeImages_f

Used to combine animations of six separate tga files into
a serials of 6x taller tga files, for preparation to roq compress

FIXME : member vars could do with more scope definition
===============
*/
void R_CombineCubeImages_f( const idCmdArgs &args ) {
	if ( args.Argc() != 2 ) {
		common->Printf( "usage: combineCubeImages <baseName>\n" );
		common->Printf( " combines basename[1-6][0001-9999].tga to basenameCM[0001-9999].tga\n" );
		common->Printf( " 1: forward 2:right 3:back 4:left 5:up 6:down\n" );
		return;
	}

	idStr	baseName = args.Argv( 1 );
	common->SetRefreshOnPrint( true );

	for ( int frameNum = 1 ; frameNum < 10000 ; frameNum++ ) {
		char	filename[MAX_IMAGE_NAME];
		byte	*pics[6];
		int		width, height;
		int		side;
		int		orderRemap[6] = { 1,3,4,2,5,6 };
		for ( side = 0 ; side < 6 ; side++ ) {
			sprintf( filename, "%s%i%04i.tga", baseName.c_str(), orderRemap[side], frameNum );

			common->Printf( "reading %s\n", filename );
			R_LoadImage( filename, &pics[side], &width, &height, NULL, true );

			if ( !pics[side] ) {
				common->Printf( "not found.\n" );
				break;
			}

			// convert from "camera" images to native cube map images
			switch( side ) {
			case 0:	// forward
				R_RotatePic( pics[side], width);
				break;
			case 1:	// back
				R_RotatePic( pics[side], width);
				R_HorizontalFlip( pics[side], width, height );
				R_VerticalFlip( pics[side], width, height );
				break;
			case 2:	// left
				R_VerticalFlip( pics[side], width, height );
				break;
			case 3:	// right
				R_HorizontalFlip( pics[side], width, height );
				break;
			case 4:	// up
				R_RotatePic( pics[side], width);
				break;
			case 5: // down
				R_RotatePic( pics[side], width);
				break;
			}
		}

		if ( side != 6 ) {
			for ( int i = 0 ; i < side ; side++ ) {
				Mem_Free( pics[side] );
			}
			break;
		}

		byte *combined = (byte *)Mem_Alloc( width*height*6*4 );
		for ( side = 0 ; side < 6 ; side++ ) {
			memcpy( combined+width*height*4*side, pics[side], width*height*4 );
			Mem_Free( pics[side] );
		}
		sprintf( filename, "%sCM%04i.tga", baseName.c_str(), frameNum );

		common->Printf( "writing %s\n", filename );
		R_WriteTGA( filename, combined, width, height*6 );

		Mem_Free( combined );
	}
	common->SetRefreshOnPrint( false );
}


/*
==================
idImage::StartBackgroundImageLoad
==================
*/
void idImage::StartBackgroundImageLoad() {
	if ( imageManager.numActiveBackgroundImageLoads >= idImageManager::MAX_BACKGROUND_IMAGE_LOADS ) {
		return;
	}
	if ( globalImages->image_showBackgroundLoads.GetBool() ) {
		common->Printf( "idImage::StartBackgroundImageLoad: %s\n", imgName.c_str() );
	}
	backgroundLoadInProgress = true;

	if ( !precompressedFile ) {
		common->Warning( "idImageManager::StartBackgroundImageLoad: %s wasn't a precompressed file", imgName.c_str() );
		return;
	}

	bglNext = globalImages->backgroundImageLoads;
	globalImages->backgroundImageLoads = this;

	char filename[MAX_IMAGE_NAME];
	ImageProgramStringToCompressedFileName( imgName, filename );

	bgl.completed = false;
	bgl.f = fileSystem->OpenFileRead( filename );
	if ( !bgl.f ) {
		common->Warning( "idImageManager::StartBackgroundImageLoad: Couldn't load %s", imgName.c_str() );
		return;
	}
	bgl.file.position = 0;
	bgl.file.length = bgl.f->Length();
	if ( bgl.file.length < sizeof( ddsFileHeader_t ) ) {
		common->Warning( "idImageManager::StartBackgroundImageLoad: %s had a bad file length", imgName.c_str() );
		return;
	}

	bgl.file.buffer = R_StaticAlloc( bgl.file.length );

	fileSystem->BackgroundDownload( &bgl );

	imageManager.numActiveBackgroundImageLoads++;

	// purge some images if necessary
	int	totalSize = 0;
	for ( idImage *check = globalImages->cacheLRU.cacheUsageNext ; check != &globalImages->cacheLRU ; check = check->cacheUsageNext ) {
		totalSize += check->StorageSize();
	}

	const int needed = this->StorageSize();

	while ( ( totalSize + needed ) > globalImages->image_cacheMegs.GetFloat() * 1024 * 1024 ) {
		// purge the least recently used
		idImage	*check = globalImages->cacheLRU.cacheUsagePrev;
		if ( check->texnum != TEXTURE_NOT_LOADED ) {
			totalSize -= check->StorageSize();
			if ( globalImages->image_showBackgroundLoads.GetBool() ) {
				common->Printf( "purging %s\n", check->imgName.c_str() );
			}
			check->PurgeImage();
		}
		// remove it from the cached list
		check->cacheUsageNext->cacheUsagePrev = check->cacheUsagePrev;
		check->cacheUsagePrev->cacheUsageNext = check->cacheUsageNext;
		check->cacheUsageNext = NULL;
		check->cacheUsagePrev = NULL;
	}
}

/*
==================
R_CompleteBackgroundImageLoads

Do we need to worry about vid_restarts here?
==================
*/
void idImageManager::CompleteBackgroundImageLoads() {
	idImage	*remainingList = NULL;
	idImage	*next;

	for ( idImage *image = backgroundImageLoads ; image ; image = next ) {
		next = image->bglNext;
		if ( image->bgl.completed ) {
			numActiveBackgroundImageLoads--;
			fileSystem->CloseFile( image->bgl.f );
			// upload the image
			image->UploadPrecompressedImage( (byte *)image->bgl.file.buffer, image->bgl.file.length );
			R_StaticFree( image->bgl.file.buffer );
			if ( image_showBackgroundLoads.GetBool() ) {
				common->Printf( "R_CompleteBackgroundImageLoad: %s\n", image->imgName.c_str() );
			}
		} else {
			image->bglNext = remainingList;
			remainingList = image;
		}
	}
	if ( image_showBackgroundLoads.GetBool() ) {
		static int prev;
		if ( numActiveBackgroundImageLoads != prev ) {
			prev = numActiveBackgroundImageLoads;
			common->Printf( "background Loads: %i\n", numActiveBackgroundImageLoads );
		}
	}

	backgroundImageLoads = remainingList;
}

/*
===============
CheckCvars
===============
*/
void idImageManager::CheckCvars() {
	// textureFilter stuff
	if ( image_filter.IsModified() || image_anisotropy.IsModified() || image_lodbias.IsModified() ) {
		ChangeTextureFilter();
		image_filter.ClearModified();
		image_anisotropy.ClearModified();
		image_lodbias.ClearModified();
	}
}

/*
===============
SumOfUsedImages
===============
*/
int idImageManager::SumOfUsedImages() {
	idImage	*image;
	int	total = 0;

	for ( int i = 0; i < images.Num(); i++ ) {
		image = images[i];
		if ( image->frameUsed == backEnd.frameCount ) {
			total += image->StorageSize();
		}
	}

	return total;
}

/*
===============
BindNull
===============
*/
void idImageManager::BindNull() {
	tmu_t	*tmu = &backEnd.glState.tmu[backEnd.glState.currenttmu];

	RB_LogComment( "BindNull()\n" );

	switch( tmu->textureType ) {
	case TT_2D:
		qglDisable( GL_TEXTURE_2D );
		break;
	case TT_CUBIC:
		qglDisable( GL_TEXTURE_CUBE_MAP );
		break;
	}
	tmu->textureType = TT_DISABLED;
}

/*
===============
Init
===============
*/
void idImageManager::Init() {

	memset(imageHashTable, 0, sizeof(imageHashTable));

	images.Resize( 1024, 1024 );

	// clear the cached LRU
	cacheLRU.cacheUsageNext = &cacheLRU;
	cacheLRU.cacheUsagePrev = &cacheLRU;

	// set default texture filter modes
	ChangeTextureFilter();

	// create built in images
	defaultImage = ImageFromFunction( "_default", R_DefaultImage );
	whiteImage = ImageFromFunction( "_white", R_WhiteImage );
	blackImage = ImageFromFunction( "_black", R_BlackImage );
	borderClampImage = ImageFromFunction( "_borderClamp", R_BorderClampImage );
	flatNormalMap = ImageFromFunction( "_flat", R_FlatNormalImage );
	ambientNormalMap = ImageFromFunction( "_ambient", R_AmbientNormalImage );
	specularTableImage = ImageFromFunction( "_specularTable", R_SpecularTableImage );
	specular2DTableImage = ImageFromFunction( "_specular2DTable", R_Specular2DTableImage );
	rampImage = ImageFromFunction( "_ramp", R_RampImage );
	alphaRampImage = ImageFromFunction( "_alphaRamp", R_RampImage );
	alphaNotchImage = ImageFromFunction( "_alphaNotch", R_AlphaNotchImage );
	fogImage = ImageFromFunction( "_fog", R_FogImage );
	fogEnterImage = ImageFromFunction( "_fogEnter", R_FogEnterImage );
	normalCubeMapImage = ImageFromFunction( "_normalCubeMap", makeNormalizeVectorCubeMap );
	noFalloffImage = ImageFromFunction( "_noFalloff", R_CreateNoFalloffImage );
	ImageFromFunction( "_quadratic", R_QuadraticImage );

	// cinematicImage is used for cinematic drawing
	// scratchImage is used for screen wipes/doublevision etc..
	cinematicImage = ImageFromFunction("_cinematic", R_RGBA8Image );
	scratchImage = ImageFromFunction("_scratch", R_RGBA8Image );
	scratchImage2 = ImageFromFunction("_scratch2", R_RGBA8Image );
	accumImage = ImageFromFunction("_accum", R_RGBA8Image );
	scratchCubeMapImage = ImageFromFunction("_scratchCubeMap", makeNormalizeVectorCubeMap );
	currentRenderImage = ImageFromFunction("_currentRender", R_RGBA8Image );
	currentDepthImage = ImageFromFunction("_currentDepth", R_RGBA8Image); // #3877. Allow shaders to access scene depth
	shadowDepthFbo = ImageFromFunction( "_shadowDepthFbo", R_RGBA8Image );
	shadowCubeMap = ImageFromFunction( "_shadowCubeMap", makeNormalizeVectorCubeMap );
	currentStencilFbo = ImageFromFunction( "_currentStencilFbo", R_RGBA8Image );
	shadowStencilFbo = ImageFromFunction( "_shadowStencilFbo", R_RGBA8Image );
	
	bloomCookedMath = ImageFromFunction( "_cookedMath", R_RGBA8Image );
	bloomImage = ImageFromFunction( "_bloomImage", R_RGBA8Image );

	cmdSystem->AddCommand( "reloadImages", R_ReloadImages_f, CMD_FL_RENDERER, "reloads images" );
	cmdSystem->AddCommand( "listImages", R_ListImages_f, CMD_FL_RENDERER, "lists images" );
	cmdSystem->AddCommand( "combineCubeImages", R_CombineCubeImages_f, CMD_FL_RENDERER, "combines six images for roq compression" );

	image_useNormalCompression.AddOnModifiedCallback( [&]() { 
		common->SetRefreshOnPrint( true );
		common->Printf("Reloading normal maps. Please wait...\n");
		idCmdArgs args( "reloadImages bump", true );
		R_ReloadImages_f( args );
		common->Printf( "Reload complete.\n" );
		common->SetRefreshOnPrint( false );
	} );
	// should forceLoadImages be here?
}

/*
===============
Shutdown
===============
*/
void idImageManager::Shutdown() {
	images.DeleteContents( true );
}

/*
====================
BeginLevelLoad

Mark all file based images as currently unused,
but don't free anything.  Calls to ImageFromFile() will
either mark the image as used, or create a new image without
loading the actual data.
====================
*/
void idImageManager::BeginLevelLoad() {
	insideLevelLoad = true;
	idImage	*image;

	for ( int i = 0 ; i < images.Num() ; i++ ) {
		image = images[ i ];

		// generator function images are always kept around
		if ( image->generatorFunction ) {
			continue;
		} else if ( com_purgeAll.GetBool() ) {
			image->PurgeImage();
		}
		image->levelLoadReferenced = false;
	}
}

/*
====================
EndLevelLoad

Free all images marked as unused, and load all images that are necessary.
This architecture prevents us from having the union of two level's
worth of data present at one time.

preload everything, never free
preload everything, free unused after level load
blocking load on demand
preload low mip levels, background load remainder on demand
====================
*/
void idImageManager::EndLevelLoad()
{
	const int start = Sys_Milliseconds();
	insideLevelLoad = false;

#ifdef MULTIPLAYER
	if (idAsyncNetwork::serverDedicated.GetInteger())
	{
		return;
	}
#endif

	common->Printf( "----- idImageManager::EndLevelLoad -----\n" );

	int	purgeCount = 0;
	int	keepCount = 0;
	int	loadCount = 0;

	// purge the ones we don't need
	for ( int i = 0 ; i < images.Num() ; i++ )
	{
		idImage	*image = images[ i ];
		if ( image->generatorFunction )
		{
			continue;
		}
		else if ( !image->levelLoadReferenced && !image->referencedOutsideLevelLoad )
		{
			//common->Printf( "Purging %s\n", image->imgName.c_str() );
			purgeCount++;
			image->PurgeImage();
		}
		else if ( image->texnum != idImage::TEXTURE_NOT_LOADED )
		{
			//common->Printf( "Keeping %s\n", image->imgName.c_str() );
			keepCount++;
		}
	}

	common->PacifierUpdate(LOAD_KEY_IMAGES_START,images.Num()/LOAD_KEY_IMAGE_GRANULARITY); // grayman #3763

	// load the ones we do need, if we are preloading
	for ( int i = 0 ; i < images.Num() ; i++ )
	{
		idImage	*image = images[ i ];
		if ( image->generatorFunction )
		{
			continue;
		}

		if ( image->levelLoadReferenced && (image->texnum == idImage::TEXTURE_NOT_LOADED) && !image->partialImage )
		{
			//common->Printf( "Loading image %d: %s\n",i,image->imgName.c_str() );
			loadCount++;
			image->ActuallyLoadImage( true, false );

			/* grayman #3763 - old way
			if ( ( loadCount & 15 ) == 0 )
			{
				session->PacifierUpdate();
			}
			*/
		}

		// grayman #3763 - update the loading bar every LOAD_KEY_IMAGE_GRANULARITY images
		if ( (i % LOAD_KEY_IMAGE_GRANULARITY) == 0)
		{
			common->PacifierUpdate(LOAD_KEY_IMAGES_INTERIM,i);
		}

	}

	const int end = Sys_Milliseconds();
	common->Printf( "%5i purged from previous\n", purgeCount );
	common->Printf( "%5i kept from previous\n", keepCount );
	common->Printf( "%5i new loaded\n", loadCount );
	common->Printf( "all images loaded in %5.1f seconds\n", ( end - start ) * 0.001f );
	common->PacifierUpdate(LOAD_KEY_DONE,0); // grayman #3763
	common->Printf( "----------------------------------------\n" );
}

/*
===============
idImageManager::StartBuild
===============
*/
void idImageManager::StartBuild() {
	ddsList.Clear();
	ddsHash.Free();
}

/*
===============
idImageManager::FinishBuild
===============
*/
void idImageManager::FinishBuild( bool removeDups ) {
	idFile *batchFile;

	if ( removeDups ) {
		ddsList.Clear();
		char *buffer = NULL;
		fileSystem->ReadFile( "makedds.bat", (void**)&buffer );
		if ( buffer ) {
			idStr str = buffer;
			while ( str.Length() ) {
				int n = str.Find( '\n' );
				if ( n > 0 ) {
					idStr line = str.Left( n + 1 );
					idStr right;
					str.Right( str.Length() - n - 1, right );
					str = right;
					ddsList.AddUnique( line );
				} else {
					break;
				}
			}
		}
	}

	batchFile = fileSystem->OpenFileWrite( ( removeDups ) ? "makedds2.bat" : "makedds.bat" );
	if ( batchFile ) {
		int ddsNum = ddsList.Num();

		for ( int i = 0; i < ddsNum; i++ ) {
			batchFile->WriteFloatString( "%s", ddsList[ i ].c_str() );
			batchFile->Printf( "@echo Finished compressing %d of %d.  %.1f percent done.\n", i+1, ddsNum, ((float)(i+1)/(float)ddsNum)*100.0f );
		}
		fileSystem->CloseFile( batchFile );
	}

	ddsList.Clear();
	ddsHash.Free();
}

/*
===============
idImageManager::AddDDSCommand
===============
*/
void idImageManager::AddDDSCommand( const char *cmd ) {

	if ( !( cmd && *cmd ) ) {
		return;
	}

	const int key = ddsHash.GenerateKey( cmd, false );
	int i;

	for ( i = ddsHash.First( key ); i != -1; i = ddsHash.Next( i ) ) {
		if ( ddsList[i].Icmp( cmd ) == 0 ) {
			break;
		}
	}

	if ( i == -1 ) {
		ddsList.Append( cmd );
	}
}

/*
===============
idImageManager::PrintMemInfo
===============
*/
void idImageManager::PrintMemInfo( MemInfo_t *mi ) {
	int i, j, total = 0;
	int *sortIndex;
	idFile *f;

	f = fileSystem->OpenFileWrite( mi->filebase + "_images.txt" );
	if ( !f ) {
		return;
	}

	// sort first
	sortIndex = new int[images.Num()];

	for ( i = 0; i < images.Num(); i++ ) {
		sortIndex[i] = i;
	}

	for ( i = 0; i < images.Num() - 1; i++ ) {
		for ( j = i + 1; j < images.Num(); j++ ) {
			if ( images[sortIndex[i]]->StorageSize() < images[sortIndex[j]]->StorageSize() ) {
				int temp = sortIndex[i];
				sortIndex[i] = sortIndex[j];
				sortIndex[j] = temp;
			}
		}
	}

	// print next
	for ( i = 0; i < images.Num(); i++ ) {
		idImage *im = images[sortIndex[i]];
		int size;

		size = im->StorageSize();
		total += size;

		f->Printf( "%s %3i %s\n", idStr::FormatNumber( size ).c_str(), im->refCount, im->imgName.c_str() );
	}

	delete[] sortIndex;
	mi->imageAssetsTotal = total;

	f->Printf( "\nTotal image bytes allocated: %s\n", idStr::FormatNumber( total ).c_str() );
	fileSystem->CloseFile( f );
}