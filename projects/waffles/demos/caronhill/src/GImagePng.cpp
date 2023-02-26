// -------------------------------------------------------------
// The contents of this file may be distributed under the CC0
// license (http://creativecommons.org/publicdomain/zero/1.0/).
// Of course you may also distribute it under a more restrictive
// license, such as any of the OSI-approved licenses
// (http://www.opensource.org/licenses).
// -------------------------------------------------------------

#ifdef WINDOWS
#	include "png.h"
#	include "zlib.h"
#else
#	include <png.h>
#	include <zlib.h>
#endif
#include <WClasses/GError.h>
#include <WClasses/GImage.h>
#include <WClasses/GHolders.h>
#include <WClasses/GFile.h>
#include <WClasses/GBits.h>
#include <fmt/core.h>

namespace GClasses {

class GPNGReader
{
public:
	png_structp m_pReadStruct;
	png_infop m_pInfoStruct;
	png_infop m_pEndInfoStruct;
	const unsigned char* m_pData;
	int m_nPos;

	GPNGReader(const unsigned char* pData)
	{
		m_pData = pData;
		m_nPos = 0;
		m_pReadStruct = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if(!m_pReadStruct)
		{
			m_pReadStruct = NULL;
			throw Ex("Failed to create read struct");
			return;	
		}
		m_pInfoStruct = png_create_info_struct(m_pReadStruct);
		if (!m_pInfoStruct) {
			png_destroy_read_struct(&m_pReadStruct, (png_infopp)0, (png_infopp)0);
			throw Ex("Failed to create info struct");
			return; //Do your own error recovery/handling here
		}
		// m_pEndInfoStruct = png_create_info_struct(m_pReadStruct);
	}

	~GPNGReader()
	{
		if(m_pReadStruct)
			png_destroy_read_struct(&m_pReadStruct, &m_pInfoStruct, &m_pEndInfoStruct);
	}

	void ReadBytes(unsigned char* pBuf, int nBytes)
	{
		memcpy(pBuf, m_pData + m_nPos, nBytes);
		m_nPos += nBytes;
	}
};

void readFunc(png_struct* pReadStruct, png_bytep pBuf, png_size_t nSize)
{
	GPNGReader* pReader = (GPNGReader*)png_get_io_ptr(pReadStruct);
	pReader->ReadBytes((unsigned char*)pBuf, (int)nSize);
}

const size_t PNGSIGSIZE = 8;

struct pngtest_error_parameters
{
   const char *file_name;
};

pngtest_error_parameters error_parameters;
/* This function is called when there is a warning, but the library thinks
 * it can continue anyway.  Replacement functions don't have to do anything
 * here if you don't want to.  In the default configuration, png_ptr is
 * not used, but it is passed in case it may be useful.
 */

static int error_count = 0; /* count calls to png_error */
static int warning_count = 0; /* count calls to png_warning */

static void PNGCBAPI
pngtest_warning(png_structp png_ptr, png_const_charp message)
{
   const char *name = "UNKNOWN (ERROR!)";
   pngtest_error_parameters *test =
      (pngtest_error_parameters*)png_get_error_ptr(png_ptr);

   ++warning_count;

   if (test != NULL && test->file_name != NULL)
      name = test->file_name;

	fmt::println("\n{}: libpng warning: {}", name, message);
}


/* This is the default error handling function.  Note that replacements for
 * this function MUST NOT RETURN, or the program will likely crash.  This
 * function is used by default, or if the program supplies NULL for the
 * error function pointer in png_set_error_fn().
 */
static void PNGCBAPI
pngtest_error(png_structp png_ptr, png_const_charp message)
{
   ++error_count;

   pngtest_warning(png_ptr, message);
   /* We can return because png_error calls the default handler, which is
    * actually OK in this case.
    */
}

bool validate(std::istream& source) {

    //Allocate a buffer of 8 bytes, where we can put the file signature.
    png_byte pngsig[PNGSIGSIZE];
    int is_png = 0;

    //Read the 8 bytes from the stream into the sig buffer.
    source.read((char*)pngsig, PNGSIGSIZE);

    //Check if the read worked...
    if (!source.good()) return false;

    //Let LibPNG check the sig. If this function returns 0, everything is OK.
    is_png = png_sig_cmp(pngsig, 0, PNGSIGSIZE);
    return (is_png == 0);
}
void userReadData(png_structp pngPtr, png_bytep data, png_size_t length) {
    //Here we get our IO pointer back from the read struct.
    //This is the parameter we passed to the png_set_read_fn() function.
    //Our std::istream pointer.
    png_voidp a = png_get_io_ptr(pngPtr);
    //Cast the pointer to std::istream* and read 'length' bytes into 'data'
    ((std::istream*)a)->read((char*)data, length);
}

void loadPng(GImage* pImage, const char* szFilename)
{
	size_t nSize;
	// const unsigned char* pData = (const unsigned char*)GFile::loadFile(szFilename, &nSize);
	std::ifstream filestream;
	filestream.exceptions(std::ios::badbit | std::ios::failbit);
	filestream.open(szFilename, std::ios::binary);
	validate(filestream);
	// ArrayHolder<char> hRawData(pData);
	// Check for the PNG signature
	// Check for the PNG signature
	// if (nDataSize < 8) {
	// 	throw Ex("file too small");
	// }
	// if(png_sig_cmp((png_bytep)pData, 0, PNGSIGSIZE) != 0) {
	// 	throw Ex("not a png file");
	// }

	
	// Read all PNG data up until the image data chunk.
	// GPNGReader reader(0);

	//Here we create the png read struct. The 3 NULL's at the end can be used
	//for your own custom error handling functions, but we'll just use the default.
	//if the function fails, NULL is returned. Always check the return values!
	png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!pngPtr) {
		std::cerr << "ERROR: Couldn't initialize png read struct" << std::endl;
		return; //Do your own error recovery/handling here
	}

	fmt::println("png_set_error_fn");
   png_set_error_fn(pngPtr, &error_parameters, pngtest_error,
       pngtest_warning);

	//Here we create the png info struct.
	//Note that this time, if this function fails, we have to clean up the read struct!
	png_infop infoPtr = png_create_info_struct(pngPtr);
	if (!infoPtr) {
		std::cerr << "ERROR: Couldn't initialize png info struct" << std::endl;
		png_destroy_read_struct(&pngPtr, (png_infopp)0, (png_infopp)0);
		return; //Do your own error recovery/handling here
	}

	//Here I've defined 2 pointers up front, so I can use them in error handling.
	//I will explain these 2 later. Just making sure these get deleted on error.
	png_bytep* rowPtrs = NULL;
	char* data = NULL;

	if (setjmp(png_jmpbuf(pngPtr))) {
		//An error occured, so clean up what we have allocated so far...
		png_destroy_read_struct(&pngPtr, &infoPtr,(png_infopp)0);
		if (rowPtrs != NULL) delete [] rowPtrs;
		if (data != NULL) delete [] data;

		std::cout << "ERROR: An error occured while reading the PNG file" << std::endl;

		//Make sure you return here. libPNG will jump to here if something
		//goes wrong, and if you continue with your normal code, you might
		//End up with an infinite loop.
		return; // Do your own error handling here.
	}

	// png_set_read_fn(reader.m_pReadStruct, (png_voidp)&reader, (png_rw_ptr)readFunc);
	png_set_read_fn(pngPtr,(png_voidp)&filestream, userReadData);

	//  //Set the amount signature bytes we've already read:
    // //We've defined PNGSIGSIZE as 8;
    png_set_sig_bytes(pngPtr, PNGSIGSIZE);
	fmt::println("Attempting png_read_info");
	   png_set_keep_unknown_chunks(pngPtr, PNG_HANDLE_CHUNK_ALWAYS,
       NULL, 0);
	png_read_info(pngPtr, infoPtr);

	// Get the image data
	int depth, color;
	png_uint_32 width, height;
	int filter_method;
	int interlace_method;
	int compression_method;

	if (png_get_IHDR(pngPtr, infoPtr, &width, &height, &depth, &color, &filter_method, &interlace_method, &compression_method) == 0) {
		fmt::println("error getting png_get_IHDR");
	}
	fmt::print("width {} height {} depth {} color {:b} filter_method {} interlace method {} compression method {}\n",width,height,depth,color,filter_method,interlace_method,compression_method);
	GAssert(depth == 8); // unexpected depth
	pImage->setSize(width, height);

	// Set gamma correction
	double dGamma;
	if (png_get_gAMA(pngPtr, infoPtr, &dGamma))
		png_set_gamma(pngPtr, 2.2, dGamma);
	else
		png_set_gamma(pngPtr, 2.2, 1.0 / 2.2); // 1.0 = viewing gamma, 2.2 = screen gamma

	fmt::print("before png_read_update_info\n");
	// Update the 'info' struct with the gamma information
	png_read_update_info(pngPtr, infoPtr);
	fmt::print("after png_read_update_info\n");
	fmt::print("before png_set_expand\n");

    png_color_16 my_background;
    png_color_16p image_background;

    // if (png_get_bKGD(reader.m_pReadStruct, reader.m_pInfoStruct, &image_background))
    //     png_set_background(reader.m_pReadStruct, image_background,
    //       PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
    // else
    //     png_set_background(reader.m_pReadStruct, &my_background,
    //       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);

	// Tell it to expand palettes to full channels
	// png_set_expand(pngPtr);
	// fmt::print("after png_set_expand\n");
	// png_set_gray_to_rgb(pngPtr);
	// fmt::print("after png_set_gray_to_rgb\n");

	// Allocate the row pointers
	fmt::print("before png_get_rowbytes\n");;
	unsigned long rowbytes = png_get_rowbytes(pngPtr, infoPtr);
	fmt::print("after png_get_rowbytes\n");
	unsigned long channels = rowbytes / width;
	ArrayHolder<unsigned char> hData(new unsigned char[rowbytes * height]);
	png_bytep pRawData = (png_bytep)hData.get();
	unsigned int i;
	{
		ArrayHolder<unsigned char> hRows(new unsigned char[sizeof(png_bytep) * height]);
		png_bytep* pRows = (png_bytep*)hRows.get();
		for(i = 0; i < height; i++)
			pRows[i] = pRawData + i * rowbytes;
		fmt::print("before png_read_image\n");;
		png_read_image(pngPtr, pRows);
		fmt::print("after png_read_image\n");
	}
	// Copy to the GImage
	unsigned long nPixels = width * height;
	unsigned int* pRGBQuads = pImage->pixels();
	unsigned char *pBytes = pRawData;
	if(channels > 3)
	{
		GAssert(channels == 4); // unexpected number of channels
		for(i = 0; i < nPixels; i++)
		{
			*pRGBQuads = gARGB(pBytes[3], pBytes[0], pBytes[1], pBytes[2]);
			pBytes += channels;
			pRGBQuads++;
		}
	}
	else if(channels == 3)
	{
		for(i = 0; i < nPixels; i++)
		{
			*pRGBQuads = gARGB(0xff, pBytes[0], pBytes[1], pBytes[2]);
			pBytes += channels;
			pRGBQuads++;
		}
	}
	else
	{
		throw Ex("Sorry, loading ", to_str(channels), "-channel pngs not supported");
/*		GAssert(channels == 1); // unexpected number of channels
		for(i = 0; i < nPixels; i++)
		{
			*pRGBQuads = gARGB(0xff, pBytes[0], pBytes[0], pBytes[0]);
			pBytes += channels;
			pRGBQuads++;
		}*/
	}
	// Check for additional tags
	png_read_end(pngPtr, infoPtr);
}

// -----------------------------------------------------------------------

class GPNGWriter
{
public:
	png_structp m_pWriteStruct;
	png_infop m_pInfoStruct;

	GPNGWriter()
	{
		m_pWriteStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, error_handler, NULL);
		if(!m_pWriteStruct)
			throw Ex("Failed to create write struct. Out of mem?");
		m_pInfoStruct = png_create_info_struct(m_pWriteStruct);
		if(!m_pInfoStruct)
			throw Ex("Failed to create info struct. Out of mem?");
	}

	~GPNGWriter()
	{
		png_destroy_write_struct(&m_pWriteStruct, &m_pInfoStruct);
	}

	static void error_handler(png_structp png_ptr, png_const_charp msg)
	{
		throw Ex("Error writing PNG file: ", msg);
	}
};


void savePng(GImage* pImage, FILE* pFile, bool bIncludeAlphaChannel)
{
	// Set the jump value (This has something to do with enabling the error handler)
	GPNGWriter writer;
	// if(setjmp(png_jmpbuf(writer.m_pWriteStruct)))
	// 	throw Ex("Failed to set the jump value");

	// Init the IO
	png_init_io(writer.m_pWriteStruct, pFile);
	// png_set_compression_level(writer.m_pWriteStruct, Z_BEST_COMPRESSION);

	// Write image stats and settings
	unsigned long width = pImage->width();
	unsigned long height = pImage->height();
	// png_set_IHDR(writer.m_pWriteStruct, writer.m_pInfoStruct,
	// 	width, height, 8,
	// 	bIncludeAlphaChannel ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB,
	// 	PNG_INTERLACE_NONE,	PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	// png_write_info(writer.m_pWriteStruct, writer.m_pInfoStruct);
	// png_set_packing(writer.m_pWriteStruct);

	// Write the image data
	unsigned long channels = bIncludeAlphaChannel ? 4 : 3;
	unsigned long rowbytes = width * channels;
	unsigned char* pRow = new unsigned char[rowbytes];
	ArrayHolder<unsigned char> hRow(pRow);
	unsigned int* pPix = pImage->pixels();
	if(channels == 4)
	{
		for(unsigned int i = 0; i < height; i++)
		{
			unsigned char* pBytes = pRow;
			for(unsigned int j = 0; j < width; j++)
			{
				*(pBytes++) = gRed(*pPix);
				*(pBytes++) = gGreen(*pPix);
				*(pBytes++) = gBlue(*pPix);
				*(pBytes++) = gAlpha(*pPix);
				pPix++;
			}
			png_write_row(writer.m_pWriteStruct, pRow);
		}
	}
	else if(channels == 3)
	{
		for(unsigned int i = 0; i < height; i++)
		{
			unsigned char* pBytes = pRow;
			for(unsigned int j = 0; j < width; j++)
			{
				*(pBytes++) = gRed(*pPix);
				*(pBytes++) = gGreen(*pPix);
				*(pBytes++) = gBlue(*pPix);
			}
			png_write_row(writer.m_pWriteStruct, pRow);
		}
	}
	else
		throw Ex("Unsupported number of channels");
	png_write_end(writer.m_pWriteStruct, writer.m_pInfoStruct);
}


// void loadPngFromHex(GImage* pImage, const char* szHex)
// {
// 	size_t len = strlen(szHex);
// 	unsigned char* pBuf = new unsigned char[len / 2];
// 	ArrayHolder<unsigned char> hBuf(pBuf);
// 	GBits::hexToBuffer(szHex, len, pBuf);
// 	loadPng(pImage, pBuf, len / 2);
// }

void savePng(GImage* pImage, FILE* pFile)
{
	savePng(pImage, pFile, true);
}

void savePng(GImage* pImage, const char* szFilename)
{
	FILE* pFile = fopen(szFilename, "wb");
	if(!pFile)
		throw Ex("Failed to create file: ", szFilename);
	FileHolder hFile(pFile);
	savePng(pImage, pFile);
}

} // namespace GClasses
