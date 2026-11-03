
#include "DocxFactory/opc/OpcPackage.h"
#include "DocxFactory/opc/OpcPart.h"
#include "DocxFactory/opc/OpcImageCache.h"
#include "DocxFactory/opc/OpcImageFile.h"
#include "DocxFactory/opc/OpcFunc.h"

#include "DocxFactory/zip/ZipFile.h"
#include "DocxFactory/os/OsFunc.h"
#include "DocxFactory/os/UnsupportedFileTypeException.h"

#include "DocxFactory/str/StrFunc.h"
#include "DocxFactory/util/DocxFactoryDefs.h"

#include "zlib/zip.h"

#if defined( DOCXFACTORY_HAVE_IMAGEMAGICK )
	#include "Magick++.h"
	#include "magick/image.h"
#endif

#include <cstring>
#include <fstream>
#include <vector>

using namespace DocxFactory;
using namespace std;



#if !defined( DOCXFACTORY_HAVE_IMAGEMAGICK )

namespace
{
	struct ImageInfo
	{
		unsigned int	m_width;
		unsigned int	m_height;
		double			m_xDpi;
		double			m_yDpi;
		bool			m_hasDpi;
	};

	static unsigned int readBE32( const unsigned char* p_buf )
	{
		return ( ( unsigned int ) p_buf[0] << 24 )
			 | ( ( unsigned int ) p_buf[1] << 16 )
			 | ( ( unsigned int ) p_buf[2] <<  8 )
			 | ( ( unsigned int ) p_buf[3] );
	}

	static unsigned int readLE32( const unsigned char* p_buf )
	{
		return ( ( unsigned int ) p_buf[3] << 24 )
			 | ( ( unsigned int ) p_buf[2] << 16 )
			 | ( ( unsigned int ) p_buf[1] <<  8 )
			 | ( ( unsigned int ) p_buf[0] );
	}

	static unsigned int readLE16( const unsigned char* p_buf )
	{
		return ( ( unsigned int ) p_buf[1] << 8 ) | p_buf[0];
	}

	static unsigned int readBE16( const unsigned char* p_buf )
	{
		return ( ( unsigned int ) p_buf[0] << 8 ) | p_buf[1];
	}

	static bool getPngInfo( ifstream& p_stream, ImageInfo& p_info )
	{
		unsigned char l_sig[8];

		if ( !p_stream.read( ( char* ) l_sig, 8 )
		  || memcmp( l_sig, "\x89PNG\r\n\x1a\n", 8 ) != 0 )
			return false;

		while ( p_stream )
		{
			unsigned char	l_hdr[8];
			unsigned int	l_len;
			char			l_type[5];
			bool			l_ihdr	= false;
			bool			l_phys	= false;

			if ( !p_stream.read( ( char* ) l_hdr, 8 ) )
				return false;

			l_len = readBE32( l_hdr );

			memcpy( l_type, l_hdr + 4, 4 );
			l_type[4] = 0;

			if ( strcmp( l_type, "IHDR" ) == 0 && l_len >= 13 )
			{
				unsigned char l_data[13];

				if ( !p_stream.read( ( char* ) l_data, 13 ) )
					return false;

				p_info.m_width	= readBE32( l_data );
				p_info.m_height	= readBE32( l_data + 4 );
				l_ihdr			= true;

				p_stream.seekg( l_len - 13, ios::cur );
			}

			else
			if ( strcmp( l_type, "pHYs" ) == 0 && l_len >= 9 )
			{
				unsigned char l_data[9];

				if ( !p_stream.read( ( char* ) l_data, 9 ) )
					return false;

				if ( l_data[8] == 1 )
				{
					unsigned int l_x = readBE32( l_data );
					unsigned int l_y = readBE32( l_data + 4 );

					if ( l_x && l_y )
					{
						p_info.m_xDpi	= l_x * 0.0254;
						p_info.m_yDpi	= l_y * 0.0254;
						p_info.m_hasDpi	= true;
					}
				}

				p_stream.seekg( l_len - 9, ios::cur );
				l_phys = true;
			}

			else
				p_stream.seekg( l_len, ios::cur );

			p_stream.seekg( 4, ios::cur ); // crc

			if ( p_info.m_width && p_info.m_height && ( l_phys || strcmp( l_type, "IDAT" ) == 0 ) )
				break;

			if ( strcmp( l_type, "IEND" ) == 0 )
				break;
		}

		return p_info.m_width && p_info.m_height;
	} // getPngInfo

	static bool getJpegInfo( ifstream& p_stream, ImageInfo& p_info )
	{
		unsigned char l_ch;

		if ( !p_stream.read( ( char* ) &l_ch, 1 ) || l_ch != 0xFF )
			return false;

		if ( !p_stream.read( ( char* ) &l_ch, 1 ) || l_ch != 0xD8 )
			return false;

		while ( p_stream )
		{
			int				l_marker;
			unsigned char	l_lenBuf[2];
			unsigned int	l_len;

			if ( !p_stream.read( ( char* ) &l_ch, 1 ) )
				return false;

			if ( l_ch != 0xFF )
				continue;

			do
			{
				if ( !p_stream.read( ( char* ) &l_ch, 1 ) )
					return false;
			}
			while ( l_ch == 0xFF );

			l_marker = l_ch;

			if ( l_marker == 0x01
			  || ( l_marker >= 0xD0 && l_marker <= 0xD9 ) )
				continue;

			if ( l_marker == 0xDA ) // start of scan
				break;

			if ( !p_stream.read( ( char* ) l_lenBuf, 2 ) )
				return false;

			l_len = readBE16( l_lenBuf );

			if ( l_len < 2 )
				return false;

			if ( l_marker == 0xE0 ) // APP0 / JFIF
			{
				vector<unsigned char> l_data( l_len - 2 );

				if ( !l_data.empty()
				  && !p_stream.read( ( char* ) l_data.data(), l_data.size() ) )
					return false;

				if ( l_data.size() >= 12
				  && memcmp( l_data.data(), "JFIF\0", 5 ) == 0 )
				{
					unsigned int l_units	= l_data[7];
					unsigned int l_x		= readBE16( l_data.data() + 8 );
					unsigned int l_y		= readBE16( l_data.data() + 10 );

					if ( l_x && l_y && ( l_units == 1 || l_units == 2 ) )
					{
						p_info.m_xDpi	= ( l_units == 2 ? l_x * 2.54 : l_x );
						p_info.m_yDpi	= ( l_units == 2 ? l_y * 2.54 : l_y );
						p_info.m_hasDpi	= true;
					}
				}
			}

			else
			if ( ( l_marker >= 0xC0 && l_marker <= 0xC3 )
			  || ( l_marker >= 0xC5 && l_marker <= 0xC7 )
			  || ( l_marker >= 0xC9 && l_marker <= 0xCB )
			  || ( l_marker >= 0xCD && l_marker <= 0xCF ) )
			{
				vector<unsigned char> l_data( l_len - 2 );

				if ( !l_data.empty()
				  && !p_stream.read( ( char* ) l_data.data(), l_data.size() ) )
					return false;

				if ( l_data.size() >= 5 )
				{
					p_info.m_height	= readBE16( l_data.data() + 1 );
					p_info.m_width	= readBE16( l_data.data() + 3 );
				}
			}

			else
				p_stream.seekg( l_len - 2, ios::cur );

			if ( p_info.m_width && p_info.m_height )
				break;
		}

		return p_info.m_width && p_info.m_height;
	} // getJpegInfo

	static bool getGifInfo( ifstream& p_stream, ImageInfo& p_info )
	{
		unsigned char l_hdr[10];

		if ( !p_stream.read( ( char* ) l_hdr, 10 ) )
			return false;

		if ( memcmp( l_hdr, "GIF87a", 6 ) != 0
		  && memcmp( l_hdr, "GIF89a", 6 ) != 0 )
			return false;

		p_info.m_width	= readLE16( l_hdr + 6 );
		p_info.m_height	= readLE16( l_hdr + 8 );

		return p_info.m_width && p_info.m_height;
	} // getGifInfo

	static bool getTiffInfo( ifstream& p_stream, ImageInfo& p_info )
	{
		unsigned char	l_hdr[8];
		bool			l_le;
		unsigned int	l_magic;
		unsigned int	l_ifdOffset;
		unsigned char	l_cntBuf[2];
		unsigned int	l_cnt;
		unsigned int	l_unit = 2;
		double			l_xRes = 0;
		double			l_yRes = 0;

		if ( !p_stream.read( ( char* ) l_hdr, 8 ) )
			return false;

		if ( l_hdr[0] == 'I' && l_hdr[1] == 'I' )		l_le = true;
		else if ( l_hdr[0] == 'M' && l_hdr[1] == 'M' )	l_le = false;
		else return false;

		l_magic = ( l_le ? readLE16( l_hdr + 2 ) : readBE16( l_hdr + 2 ) );

		if ( l_magic != 42 )
			return false;

		l_ifdOffset = ( l_le ? readLE32( l_hdr + 4 ) : readBE32( l_hdr + 4 ) );

		p_stream.seekg( l_ifdOffset );

		if ( !p_stream.read( ( char* ) l_cntBuf, 2 ) )
			return false;

		l_cnt = ( l_le ? readLE16( l_cntBuf ) : readBE16( l_cntBuf ) );

		for ( unsigned int l_i = 0; l_i < l_cnt; l_i++ )
		{
			unsigned char	l_entry[12];
			unsigned int	l_tag;
			unsigned int	l_type;
			unsigned int	l_val;

			if ( !p_stream.read( ( char* ) l_entry, 12 ) )
				return false;

			l_tag	= ( l_le ? readLE16( l_entry )		: readBE16( l_entry ) );
			l_type	= ( l_le ? readLE16( l_entry + 2 )	: readBE16( l_entry + 2 ) );
			l_val	= ( l_le ? readLE32( l_entry + 8 )	: readBE32( l_entry + 8 ) );

			if ( l_tag == 256 || l_tag == 257 )
			{
				unsigned int l_v = ( l_type == 3
					? ( l_le ? readLE16( l_entry + 8 ) : readBE16( l_entry + 8 ) )
					: l_val );

				if ( l_tag == 256 )	p_info.m_width	= l_v;
				else				p_info.m_height	= l_v;
			}

			else
			if ( l_tag == 282 || l_tag == 283 )
			{
				streampos		l_pos = p_stream.tellg();
				unsigned char	l_rat[8];

				p_stream.seekg( l_val );

				if ( p_stream.read( ( char* ) l_rat, 8 ) )
				{
					unsigned int l_num = ( l_le ? readLE32( l_rat )		: readBE32( l_rat ) );
					unsigned int l_den = ( l_le ? readLE32( l_rat + 4 )	: readBE32( l_rat + 4 ) );

					if ( l_den )
					{
						if ( l_tag == 282 )	l_xRes = ( double ) l_num / l_den;
						else				l_yRes = ( double ) l_num / l_den;
					}
				}

				p_stream.seekg( l_pos );
			}

			else
			if ( l_tag == 296 )
				l_unit = ( l_type == 3
					? ( l_le ? readLE16( l_entry + 8 ) : readBE16( l_entry + 8 ) )
					: l_val );
		}

		if ( !p_info.m_width || !p_info.m_height )
			return false;

		if ( l_xRes > 0 && l_yRes > 0 && l_unit != 1 )
		{
			if ( l_unit == 3 ) // per cm
			{
				l_xRes *= 2.54;
				l_yRes *= 2.54;
			}

			p_info.m_xDpi	= l_xRes;
			p_info.m_yDpi	= l_yRes;
			p_info.m_hasDpi	= true;
		}

		return true;
	} // getTiffInfo

	static bool getImageInfo( const string& p_fileName, ImageInfo& p_info )
	{
		ifstream	l_stream( p_fileName.c_str(), ios::binary );

		p_info.m_width	= 0;
		p_info.m_height	= 0;
		p_info.m_xDpi	= 0;
		p_info.m_yDpi	= 0;
		p_info.m_hasDpi	= false;

		if ( !l_stream )
			return false;

		if ( getPngInfo( l_stream, p_info ) )
			return true;

		p_info.m_width	= 0;
		p_info.m_height	= 0;
		l_stream.clear();
		l_stream.seekg( 0 );

		if ( getJpegInfo( l_stream, p_info ) )
			return true;

		p_info.m_width	= 0;
		p_info.m_height	= 0;
		l_stream.clear();
		l_stream.seekg( 0 );

		if ( getGifInfo( l_stream, p_info ) )
			return true;

		p_info.m_width	= 0;
		p_info.m_height	= 0;
		l_stream.clear();
		l_stream.seekg( 0 );

		return getTiffInfo( l_stream, p_info );
	} // getImageInfo
}

#endif



OpcImageFile::OpcImageFile(
	OpcImageCache*	p_imageCache,
	const string&	p_sourceFullPath,
	const string&	p_fileExt )
{
	string	l_ext;
	bool	l_convert = false;

		 if ( p_fileExt == ".jpeg" )	  l_ext = ".jpeg";
	else if	( p_fileExt == ".jpg" )		  l_ext = ".jpeg";
	else if	( p_fileExt == ".jpe" )		  l_ext = ".jpeg";
	else if	( p_fileExt == ".jfif" )	  l_ext = ".jpeg";
	else if ( p_fileExt == ".png" )		  l_ext = ".png";
	else if ( p_fileExt == ".gif" )		  l_ext = ".gif";
	else if ( p_fileExt == ".tiff" )	  l_ext = ".tiff";
	else if ( p_fileExt == ".tif" )		  l_ext = ".tiff";
	else if ( p_fileExt == ".bmp" )		{ l_ext = ".png";  l_convert = true; }
	else if ( p_fileExt == ".dib" )		{ l_ext = ".png";  l_convert = true; }
	else								{ l_ext = ".jpeg"; l_convert = true; }

	m_package			= p_imageCache ->getPackage();
	m_imageCache		= p_imageCache;
	m_sourceFullPath	= p_sourceFullPath;
	m_ext				= l_ext;
	m_converted			= false;

	m_imageSource		= SOURCE_EXTERNAL_FILE;
	m_imageBuf			= NULL;
	m_imageBufSize		= 0;

	m_emuWidth			= 0;
	m_emuHeight			= 0;

	if ( l_convert )
		loadImage( true, l_ext );
} // c'tor

OpcImageFile::~OpcImageFile()
{
	if ( m_imageBuf )
		delete[] m_imageBuf;
} // d'tor



void OpcImageFile::loadImage( bool p_convert, const string& p_convertTo /* = "" */ )
{
#if !defined( DOCXFACTORY_HAVE_IMAGEMAGICK )

	ImageInfo l_info;

	if ( p_convert )
		throw UnsupportedFileTypeException( m_sourceFullPath, "png, jpeg, gif or tiff", __FILE__, __LINE__ );

	if ( !getImageInfo( m_sourceFullPath, l_info ) )
		throw UnsupportedFileTypeException( m_sourceFullPath, "png, jpeg, gif or tiff", __FILE__, __LINE__ );

	if ( l_info.m_hasDpi )
	{
		m_emuWidth	= ( unsigned int ) ( ( double ) l_info.m_width	/ l_info.m_xDpi * m_emuPerInch );
		m_emuHeight	= ( unsigned int ) ( ( double ) l_info.m_height	/ l_info.m_yDpi * m_emuPerInch );
	}

	else
	{
		m_emuWidth	= l_info.m_width	* m_emuPerPixel;
		m_emuHeight	= l_info.m_height	* m_emuPerPixel;
	}

#else

	Magick::Image*	l_image		= NULL;
	Magick::Blob*	l_imageBlob	= NULL;

	string	l_ext;
	double	l_width;
	double	l_height;

	l_ext = StrFunc::caps( p_convertTo );

	if ( l_ext.length() > 0
	  && l_ext[0] == '.' )
		 l_ext = l_ext.substr( 1 );

	try
	{
		// if we try to load an image of unsupported format ErrorCorruptImage will be thrown
		l_image		= new Magick::Image( m_sourceFullPath );
		l_imageBlob	= new Magick::Blob();

		// resolutionUnits() gets undefined value after reading the image (possibly a bug in ImageMagick)
		// so we set it with Image::constImage() ->units which gets the correct value
		l_image ->resolutionUnits( l_image ->constImage() ->units );

		if ( p_convert )
		{
			try
			{
				l_image ->magick( l_ext );
				l_image ->write( l_imageBlob );

				m_converted = true;
			}

			catch ( ... )
			{
				if ( l_image )		delete l_image;
				if ( l_imageBlob )	delete l_imageBlob;

				l_image		= new Magick::Image( m_sourceFullPath );
				l_imageBlob	= new Magick::Blob();

				l_image ->resolutionUnits( l_image ->constImage() ->units );
				l_image ->write( l_imageBlob );
			}
		}

		else
			l_image ->write( l_imageBlob );

		if ( m_imageBuf )
			delete[] m_imageBuf; // in case loadImage() is called more than once

		m_imageBufSize	= l_imageBlob ->length();
		m_imageBuf		= new byte[ m_imageBufSize ];

		memcpy( m_imageBuf, l_imageBlob ->data(), m_imageBufSize );
		m_imageSource = SOURCE_IMAGE_OBJECT; // mark image as loaded

		if ( l_image ->xResolution() == 0
		  || l_image ->yResolution() == 0 )
		{
			m_emuWidth	= ( unsigned int ) ( l_image ->columns()	* m_emuPerPixel );
			m_emuHeight	= ( unsigned int ) ( l_image ->rows()		* m_emuPerPixel );
		}

		else
		{
			// calculate print size (the units are l_image ->resolutionUnits())
			l_width		= ( double ) l_image ->columns()	/ l_image ->xResolution();
			l_height	= ( double ) l_image ->rows()		/ l_image ->yResolution();
		
			// multiply by the emu conversion scalar
			// according to the resolution units to get emu size
			// if resolutionUnits() is undefined we treat it as centimeters
			// like image magick does when converting

			if ( l_image ->resolutionUnits() == Magick::PixelsPerInchResolution )
			{
				m_emuWidth	= ( unsigned int ) ( l_width	* m_emuPerInch );
				m_emuHeight	= ( unsigned int ) ( l_height	* m_emuPerInch );
			}

			else
			{
				m_emuWidth	= ( unsigned int ) ( l_width	* m_emuPerCm );
				m_emuHeight	= ( unsigned int ) ( l_height	* m_emuPerCm );
			}
		}

		delete l_image;
		delete l_imageBlob;
	}

	catch ( ... )
	{
		if ( l_image )		delete		l_image;
		if ( l_imageBlob )	delete		l_imageBlob;
		if ( m_imageBuf )	delete[]	m_imageBuf;

		throw;
	}

#endif
} // loadImage



void OpcImageFile::save( const string& p_partFullPath )
{
	if ( m_imageSource == SOURCE_EXTERNAL_FILE )
		m_package ->getZipFile() ->addEntryFromFile( m_sourceFullPath,
			OpcFunc::opcToZipPath( p_partFullPath ) );

	else
		m_package ->getZipFile() ->addEntryFromBuf(
			OpcFunc::opcToZipPath( p_partFullPath ), m_imageBuf, m_imageBufSize );
} // save



OpcPackage* OpcImageFile::getPackage() const
{
	return m_package;
} // getPackage

OpcImageCache* OpcImageFile::getImageCache() const
{
	return m_imageCache;
} // getImageCache

string OpcImageFile::getSourceFullPath() const
{
	return m_sourceFullPath;
} // getSourceFullPath

string OpcImageFile::getExt() const
{
	return m_ext;
} // getExt

bool OpcImageFile::isConverted() const
{
	return m_converted;
} // isConverted

unsigned int OpcImageFile::getEmuWidth() const
{
	// if the image is not yet loaded into the object, the first
	// call to this function will have to load it using loadImage()
	// which is a non-const member function. we want to keep getEmuWidth()
	// a const function because the change in the state of the object
	// doesn't concern the user of the object, so we cast the const away to call loadImage().
	// the same applies for getEmuHeight().

	if ( m_imageSource == SOURCE_EXTERNAL_FILE )
		( ( OpcImageFile* ) this ) ->loadImage( false );
	
	return m_emuWidth;
} // getPixelWidth

unsigned int OpcImageFile::getEmuHeight() const
{
	if ( m_imageSource == SOURCE_EXTERNAL_FILE )
		( ( OpcImageFile* ) this ) ->loadImage( false );
	
	return m_emuHeight;
} // getPixelHeight
