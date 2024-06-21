// ==========================================================
// Color manipulation routines
//
// Design and implementation by
// - Herve Drolon (drolon@infonie.fr)
// - Carsten Klein (c.klein@datagis.com)
// - Mihail Naydenov (mnaydenov@users.sourceforge.net)
//
// This file is part of FreeImage 3
//
// COVERED CODE IS PROVIDED UNDER THIS LICENSE ON AN "AS IS" BASIS, WITHOUT WARRANTY
// OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, WITHOUT LIMITATION, WARRANTIES
// THAT THE COVERED CODE IS FREE OF DEFECTS, MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE
// OR NON-INFRINGING. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE COVERED
// CODE IS WITH YOU. SHOULD ANY COVERED CODE PROVE DEFECTIVE IN ANY RESPECT, YOU (NOT
// THE INITIAL DEVELOPER OR ANY OTHER CONTRIBUTOR) ASSUME THE COST OF ANY NECESSARY
// SERVICING, REPAIR OR CORRECTION. THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL
// PART OF THIS LICENSE. NO USE OF ANY COVERED CODE IS AUTHORIZED HEREUNDER EXCEPT UNDER
// THIS DISCLAIMER.
//
// Use at your own risk!
// ==========================================================

#include "FreeImage.h"
#include "Utilities.h"
#include <complex>
#include <cstring>
#include "../FreeImage/SimpleTools.h"

// ----------------------------------------------------------
//   Macros + structures
// ----------------------------------------------------------

#define GET_HI_NIBBLE(byte)     ((byte) >> 4)
#define SET_HI_NIBBLE(byte, n)  byte &= 0x0F, byte |= ((n) << 4)
#define GET_LO_NIBBLE(byte)     ((byte) & 0x0F)
#define SET_LO_NIBBLE(byte, n)  byte &= 0xF0, byte |= ((n) & 0x0F)
#define GET_NIBBLE(cn, byte)    ((cn) ? (GET_HI_NIBBLE(byte)) : (GET_LO_NIBBLE(byte))) 
#define SET_NIBBLE(cn, byte, n) if (cn) SET_HI_NIBBLE(byte, n); else SET_LO_NIBBLE(byte, n) 

// ----------------------------------------------------------


/** @brief Inverts each pixel data.

@param src Input image to be processed.
@return Returns TRUE if successful, FALSE otherwise.
*/
FIBOOL DLL_CALLCONV 
FreeImage_Invert(FIBITMAP *src) {

	if (!FreeImage_HasPixels(src)) return FALSE;
	
	unsigned i, x, y, k;
	
	const unsigned width = FreeImage_GetWidth(src);
	const unsigned height = FreeImage_GetHeight(src);
	const unsigned bpp = FreeImage_GetBPP(src);

	FREE_IMAGE_TYPE image_type = FreeImage_GetImageType(src);

	if (image_type == FIT_BITMAP) {
		switch (bpp) {
			case 1 :
			case 4 :
			case 8 :
			{
				// if the dib has a colormap, just invert it
				// else, keep the linear grayscale

				if (FreeImage_GetColorType(src) == FIC_PALETTE) {
					FIRGBA8 *pal = FreeImage_GetPalette(src);

					for (i = 0; i < FreeImage_GetColorsUsed(src); i++) {
						pal[i].red	= 255 - pal[i].red;
						pal[i].green = 255 - pal[i].green;
						pal[i].blue	= 255 - pal[i].blue;
					}
				} else {
					for (y = 0; y < height; y++) {
						uint8_t *bits = FreeImage_GetScanLine(src, y);

						for (x = 0; x < FreeImage_GetLine(src); x++) {
							bits[x] = ~bits[x];
						}
					}
				}

				break;
			}		

			case 24 :
			case 32 :
			{
				// Calculate the number of bytes per pixel (3 for 24-bit or 4 for 32-bit)
				const unsigned bytespp = FreeImage_GetLine(src) / width;

				for (y = 0; y < height; y++) {
					uint8_t *bits = FreeImage_GetScanLine(src, y);
					for (x = 0; x < width; x++) {
						for (k = 0; k < bytespp; k++) {
							bits[k] = ~bits[k];
						}
						bits += bytespp;
					}
				}

				break;
			}
			default:
				return FALSE;
		}
	}
	else if ((image_type == FIT_UINT16) || (image_type == FIT_RGB16) || (image_type == FIT_RGBA16)) {
		// Calculate the number of words per pixel (1 for 16-bit, 3 for 48-bit or 4 for 64-bit)
		const unsigned wordspp = (FreeImage_GetLine(src) / width) / sizeof(uint16_t);

		for (y = 0; y < height; y++) {
			auto *bits = (uint16_t*)FreeImage_GetScanLine(src, y);
			for (x = 0; x < width; x++) {
				for (k = 0; k < wordspp; k++) {
					bits[k] = ~bits[k];
				}
				bits += wordspp;
			}
		}
	}
	else {
		// anything else ... 
		return FALSE;
	}
		
	return TRUE;
}

/** @brief Perfoms an histogram transformation on a 8, 24 or 32-bit image 
according to the values of a lookup table (LUT).

The transformation is done as follows.<br>
Image 8-bit : if the image has a color palette, the LUT is applied to this palette, 
otherwise, it is applied to the grey values.<br>
Image 24-bit & 32-bit : if channel == FICC_RGB, the same LUT is applied to each color
plane (R,G, and B). Otherwise, the LUT is applied to the specified channel only.
@param src Input image to be processed.
@param LUT Lookup table. <b>The size of 'LUT' is assumed to be 256.</b>
@param channel The color channel to be processed (only used with 24 & 32-bit DIB).
@return Returns TRUE if successful, FALSE otherwise.
@see FREE_IMAGE_COLOR_CHANNEL
*/
FIBOOL DLL_CALLCONV 
FreeImage_AdjustCurve(FIBITMAP *src, uint8_t *LUT, FREE_IMAGE_COLOR_CHANNEL channel) {
	unsigned x, y;
	uint8_t *bits{};

	if (!FreeImage_HasPixels(src) || !LUT || (FreeImage_GetImageType(src) != FIT_BITMAP))
		return FALSE;

	int bpp = FreeImage_GetBPP(src);
	if ((bpp != 8) && (bpp != 24) && (bpp != 32))
		return FALSE;

	// apply the LUT
	switch (bpp) {

		case 8 :
		{
			// if the dib has a colormap, apply the LUT to it
			// else, apply the LUT to pixel values

			if (FreeImage_GetColorType(src) == FIC_PALETTE) {
				FIRGBA8 *rgb = FreeImage_GetPalette(src);
				for (unsigned pal = 0; pal < FreeImage_GetColorsUsed(src); pal++) {
					rgb->red   = LUT[rgb->red];
					rgb->green = LUT[rgb->green];
					rgb->blue  = LUT[rgb->blue];
					rgb++;
				}
			}
			else {
				for (y = 0; y < FreeImage_GetHeight(src); y++) {
					bits =  FreeImage_GetScanLine(src, y);
					for (x = 0; x < FreeImage_GetWidth(src); x++) {
						bits[x] = LUT[ bits[x] ];
					}
				}
			}

			break;
		}

		case 24 :
		case 32 :
		{
			const int bytespp = FreeImage_GetLine(src) / FreeImage_GetWidth(src);

			switch (channel) {
				case FICC_RGB :
					for (y = 0; y < FreeImage_GetHeight(src); y++) {
						bits =  FreeImage_GetScanLine(src, y);
						for (x = 0; x < FreeImage_GetWidth(src); x++) {
							bits[FI_RGBA_BLUE]	= LUT[ bits[FI_RGBA_BLUE] ];	// B
							bits[FI_RGBA_GREEN] = LUT[ bits[FI_RGBA_GREEN] ];	// G
							bits[FI_RGBA_RED]	= LUT[ bits[FI_RGBA_RED] ];		// R
							
							bits += bytespp;
						}
					}
					break;

				case FICC_BLUE :
					for (y = 0; y < FreeImage_GetHeight(src); y++) {
						bits =  FreeImage_GetScanLine(src, y);
						for (x = 0; x < FreeImage_GetWidth(src); x++) {
							bits[FI_RGBA_BLUE] = LUT[ bits[FI_RGBA_BLUE] ];		// B
							
							bits += bytespp;
						}
					}
					break;

				case FICC_GREEN :
					for (y = 0; y < FreeImage_GetHeight(src); y++) {
						bits =  FreeImage_GetScanLine(src, y);
						for (x = 0; x < FreeImage_GetWidth(src); x++) {
							bits[FI_RGBA_GREEN] = LUT[ bits[FI_RGBA_GREEN] ];	// G
							
							bits += bytespp;
						}
					}
					break;

				case FICC_RED :
					for (y = 0; y < FreeImage_GetHeight(src); y++) {
						bits =  FreeImage_GetScanLine(src, y);
						for (x = 0; x < FreeImage_GetWidth(src); x++) {
							bits[FI_RGBA_RED] = LUT[ bits[FI_RGBA_RED] ];		// R
							
							bits += bytespp;
						}
					}
					break;
					
				case FICC_ALPHA :
					if (32 == bpp) {
						for (y = 0; y < FreeImage_GetHeight(src); y++) {
							bits =  FreeImage_GetScanLine(src, y);
							for (x = 0; x < FreeImage_GetWidth(src); x++) {
								bits[FI_RGBA_ALPHA] = LUT[ bits[FI_RGBA_ALPHA] ];	// A
								
								bits += bytespp;
							}
						}
					}
					break;

				default:
					break;
			}
			break;
		}
	}

	return TRUE;
}

/** @brief Performs gamma correction on a 8, 24 or 32-bit image.

@param src Input image to be processed.
@param gamma Gamma value to use. A value of 1.0 leaves the image alone, 
less than one darkens it, and greater than one lightens it.
@return Returns TRUE if successful, FALSE otherwise.
*/
FIBOOL DLL_CALLCONV 
FreeImage_AdjustGamma(FIBITMAP *src, double gamma) {
	uint8_t LUT[256];		// Lookup table

	if (!FreeImage_HasPixels(src) || (gamma <= 0))
		return FALSE;
	
	// Build the lookup table

	double exponent = 1 / gamma;
	double v = 255.0 * (double)pow((double)255, -exponent);
	for (int i = 0; i < 256; i++) {
		double color = (double)pow((double)i, exponent) * v;
		if (color > 255)
			color = 255;
		LUT[i] = (uint8_t)floor(color + 0.5);
	}

	// Apply the gamma correction
	return FreeImage_AdjustCurve(src, LUT, FICC_RGB);
}

/** @brief Adjusts the brightness of a 8, 24 or 32-bit image by a certain amount.

@param src Input image to be processed.
@param percentage Where -100 <= percentage <= 100<br>
A value 0 means no change, less than 0 will make the image darker 
and greater than 0 will make the image brighter.
@return Returns TRUE if successful, FALSE otherwise.
*/
FIBOOL DLL_CALLCONV 
FreeImage_AdjustBrightness(FIBITMAP *src, double percentage) {
	uint8_t LUT[256];		// Lookup table
	double value;

	if (!FreeImage_HasPixels(src))
		return FALSE;
	
	// Build the lookup table
	const double scale = (100 + percentage) / 100;
	for (int i = 0; i < 256; i++) {
		value = i * scale;
		value = MAX(0.0, MIN(value, 255.0));
		LUT[i] = (uint8_t)floor(value + 0.5);
	}
	return FreeImage_AdjustCurve(src, LUT, FICC_RGB);
}

/** @brief Adjusts the contrast of a 8, 24 or 32-bit image by a certain amount.

@param src Input image to be processed.
@param percentage Where -100 <= percentage <= 100<br>
A value 0 means no change, less than 0 will decrease the contrast 
and greater than 0 will increase the contrast of the image.
@return Returns TRUE if successful, FALSE otherwise.
*/
FIBOOL DLL_CALLCONV 
FreeImage_AdjustContrast(FIBITMAP *src, double percentage) {
	uint8_t LUT[256];		// Lookup table
	double value;

	if (!FreeImage_HasPixels(src))
		return FALSE;
	
	// Build the lookup table
	const double scale = (100 + percentage) / 100;
	for (int i = 0; i < 256; i++) {
		value = 128 + (i - 128) * scale;
		value = MAX(0.0, MIN(value, 255.0));
		LUT[i] = (uint8_t)floor(value + 0.5);
	}
	return FreeImage_AdjustCurve(src, LUT, FICC_RGB);
}

/** @brief Computes image histogram

For 24-bit and 32-bit images, histogram can be computed from red, green, blue and 
black channels. For 8-bit images, histogram is computed from the black channel. Other 
bit depth is not supported (nothing is done).
@param src Input image to be processed.
@param histo Histogram array to fill. <b>The size of 'histo' is assumed to be 256.</b>
@param channel Color channel to use
@return Returns TRUE if succesful, returns FALSE if the image bit depth isn't supported.
*/
FIBOOL DLL_CALLCONV 
FreeImage_GetHistogram(FIBITMAP *src, uint32_t *histo, FREE_IMAGE_COLOR_CHANNEL channel) {
	uint8_t pixel;
	uint8_t *bits{};
	unsigned x, y;

	if (!FreeImage_HasPixels(src) || !histo) return FALSE;

	const unsigned width  = FreeImage_GetWidth(src);
	const unsigned height = FreeImage_GetHeight(src);
	const unsigned bpp    = FreeImage_GetBPP(src);

	if (bpp == 8) {
		// clear histogram array
		memset(histo, 0, 256 * sizeof(uint32_t));
		// compute histogram for black channel
		for (y = 0; y < height; y++) {
			bits = FreeImage_GetScanLine(src, y);
			for (x = 0; x < width; x++) {
				// get pixel value
				pixel = bits[x];
				histo[pixel]++;
			}
		}
		return TRUE;
	}
	else if ((bpp == 24) || (bpp == 32)) {
		int bytespp = bpp / 8;	// bytes / pixel

		// clear histogram array
		memset(histo, 0, 256 * sizeof(uint32_t));

		switch (channel) {
			case FICC_RED:
				// compute histogram for red channel
				for (y = 0; y < height; y++) {
					bits =  FreeImage_GetScanLine(src, y);
					for (x = 0; x < width; x++) {
						pixel = bits[FI_RGBA_RED];	// R
						histo[pixel]++;
						bits += bytespp;
					}
				}
				return TRUE;

			case FICC_GREEN:
				// compute histogram for green channel
				for (y = 0; y < height; y++) {
					bits =  FreeImage_GetScanLine(src, y);
					for (x = 0; x < width; x++) {
						pixel = bits[FI_RGBA_GREEN];	// G
						histo[pixel]++;
						bits += bytespp;
					}
				}
				return TRUE;

			case FICC_BLUE:
				// compute histogram for blue channel
				for (y = 0; y < height; y++) {
					bits =  FreeImage_GetScanLine(src, y);
					for (x = 0; x < width; x++) {
						pixel = bits[FI_RGBA_BLUE];	// B
						histo[pixel]++;
						bits += bytespp;
					}
				}
				return TRUE;

			case FICC_BLACK:
			case FICC_RGB:
				// compute histogram for black channel
				for (y = 0; y < height; y++) {
					bits =  FreeImage_GetScanLine(src, y);
					for (x = 0; x < width; x++) {
						// RGB to GREY conversion
						pixel = GREY(bits[FI_RGBA_RED], bits[FI_RGBA_GREEN], bits[FI_RGBA_BLUE]);
						histo[pixel]++;
						bits += bytespp;
					}
				}
				return TRUE;
				
			default:
				return FALSE;
		}
	}

	return FALSE;
}


namespace
{

	template <typename PixelSelector_>
	class HistogramBuilder
		: private PixelSelector_
	{
	public:
		HistogramBuilder(uint32_t* hist, uint32_t stride)
			: mHist(hist)
			, mStride(stride)
		{ }

		bool Discardable() const {
			return !mHist;
		}

		void SetZero(uint32_t v) const {
			mHist[0] = v;
		}

		template <typename PixelType_, typename IndexFunction_>
		void Add(const PixelType_& pixel, IndexFunction_&& indexFunction) const
		{
			const auto i = std::forward<IndexFunction_>(indexFunction)(PixelSelector_::operator()(pixel));
			++mHist[i * mStride];
		}

	private:
		uint32_t* mHist;
		uint32_t mStride;
	};

	template <typename PixelType_>
	class HistogramFloat
	{
	public:
		using ValueType = ToValueType<PixelType_>;
		static_assert(std::is_floating_point<ValueType>::value, "wrong pixel type");

		HistogramFloat(FIBITMAP* dib, uint32_t binsNumber, const ValueType& minVal, const ValueType& maxVal)
			: mBitmap(dib)
			, mBinsNumber(binsNumber)
			, mMinVal(minVal)
			, mMaxVal(maxVal)
		{ }

		HistogramFloat(const HistogramFloat&) = delete;
		HistogramFloat(HistogramFloat&&) noexcept = default;

		~HistogramFloat() = default;

		HistogramFloat& operator=(const HistogramFloat&) = delete;
		HistogramFloat& operator=(HistogramFloat&&) noexcept = default;

		template <typename... Builders_>
		bool operator()(const Builders_&... builders) const
		{
			if (mMinVal > mMaxVal || mBinsNumber < 1) {
				return false;
			}
			if (mMinVal == mMaxVal) {
				const uint32_t pixelsNumber = FreeImage_GetWidth(mBitmap) * FreeImage_GetHeight(mBitmap);
				(..., builders.SetZero(pixelsNumber));
				return true;
			}
			const ValueType div = static_cast<ValueType>(mBinsNumber) / (mMaxVal - mMinVal);

			const auto CalculateBinIndex = [&, this](const ValueType& value) {
				const uint32_t i = static_cast<uint32_t>(std::max(static_cast<ValueType>(0), (value - mMinVal) * div));
				return std::min(i, mBinsNumber - 1);
			};

			BitmapForEach<PixelType_>(mBitmap, [&](const PixelType_& p, uint32_t /*x*/, uint32_t /*y*/) {
				(..., builders.Add(p, CalculateBinIndex));
			});
			return true;
		}

	private:
		FIBITMAP* mBitmap;
		uint32_t mBinsNumber;
		ValueType mMinVal;
		ValueType mMaxVal;
	};

	template <typename PixelType_>
	class HistogramSInt
	{
		using ValueType = ToValueType<PixelType_>;
		static_assert(!std::is_floating_point<ValueType>::value, "wrong pixel type");
		static_assert(std::is_signed<ValueType>::value, "wrong pixel type");
	public:
		HistogramSInt(FIBITMAP* dib, uint32_t binsNumber)
			: mBitmap(dib)
			, mBinsNumber(binsNumber)
		{ }

		HistogramSInt(const HistogramSInt&) = delete;
		HistogramSInt(HistogramSInt&&) noexcept = default;

		~HistogramSInt() = default;

		HistogramSInt& operator=(const HistogramSInt&) = delete;
		HistogramSInt& operator=(HistogramSInt&&) noexcept = default;

		template <typename... Builders_>
		bool operator()(const Builders_&... builders) const
		{
			if (mBinsNumber < 1) {
				return false;
			}

			constexpr uint32_t bits = 8 * sizeof(ValueType);
			const auto CalculateBinIndexSigned = [&](const ValueType& value) {
				using WideValueType = ToWiderType<ValueType>;
				const auto uvalue = static_cast<ToUnsignedType<WideValueType>>(static_cast<WideValueType>(value) - std::numeric_limits<ValueType>::min());
				const uint32_t i = static_cast<uint32_t>((uvalue * mBinsNumber) >> bits);
				return std::min(i, mBinsNumber - 1);
			};

			BitmapForEach<PixelType_>(mBitmap, [&](const PixelType_& p, uint32_t /*x*/, uint32_t /*y*/) {
				(..., builders.Add(p, CalculateBinIndexSigned));
			});
			return true;
		}

	private:
		FIBITMAP* mBitmap;
		uint32_t mBinsNumber;
	};

	template <typename PixelType_>
	class HistogramUInt
	{
	public:
		using ValueType = ToValueType<PixelType_>;
		static_assert(!std::is_floating_point<ValueType>::value, "wrong pixel type");
		static_assert(std::is_unsigned<ValueType>::value, "wrong pixel type");

		HistogramUInt(FIBITMAP* dib, uint32_t binsNumber)
			: mBitmap(dib)
			, mBinsNumber(binsNumber)
		{ }

		HistogramUInt(const HistogramUInt&) = delete;
		HistogramUInt(HistogramUInt&&) noexcept = default;

		~HistogramUInt() = default;

		HistogramUInt& operator=(const HistogramUInt&) = delete;
		HistogramUInt& operator=(HistogramUInt&&) noexcept = default;

		template <typename... Builders_>
		bool operator()(const Builders_&... builders) const
		{
			if (mBinsNumber < 1) {
				return false;
			}
			constexpr uint32_t bits = 8 * sizeof(ValueType);
			if (mBinsNumber == (1ULL << bits)) {
				const auto CalculateBinIndexWithoutScale = [&](const ValueType& value) {
					const uint32_t i = static_cast<uint32_t>(value);
					return std::min(i, mBinsNumber - 1);
				};

				BitmapForEach<PixelType_>(mBitmap, [&](const PixelType_& p, uint32_t /*x*/, uint32_t /*y*/) {
					(..., builders.Add(p, CalculateBinIndexWithoutScale));
				});
			}
			else {
				const auto CalculateBinIndexWithScale = [&](const ValueType& value) {
					using WideValueType = ToWiderType<ValueType>;
					const uint32_t i = static_cast<uint32_t>((static_cast<WideValueType>(value) * mBinsNumber) >> bits);
					return std::min(i, mBinsNumber - 1);
				};

				BitmapForEach<PixelType_>(mBitmap, [&](const PixelType_& p, uint32_t /*x*/, uint32_t /*y*/) {
					(..., builders.Add(p, CalculateBinIndexWithScale));
				});
			}
			return true;
		}

	private:
		FIBITMAP* mBitmap;
		uint32_t mBinsNumber;
	};

	template <uint32_t BuildersNum_>
	struct InvokeWithBuildersImpl
	{
		template <typename Function_, typename Builder0_, typename... Args_>
		static
		bool Apply(Function_&& f, const Builder0_& b0, Args_&&... args)
		{
			if (!b0.Discardable()) {
				return InvokeWithBuildersImpl<BuildersNum_ - 1>::Apply(std::forward<Function_>(f), std::forward<Args_>(args)..., b0);
			}
			else {
				return InvokeWithBuildersImpl<BuildersNum_ - 1>::Apply(std::forward<Function_>(f), std::forward<Args_>(args)...);
			}
		}
	};

	template <>
	struct InvokeWithBuildersImpl<0>
	{
		template <typename Function_, typename... Args_>
		static
		bool Apply(Function_&& f, Args_&&... args)
		{
			return std::forward<Function_>(f)(std::forward<Args_>(args)...);
		}
	};

	template <typename Function_, typename... Builders_>
	bool InvokeWithBuilders(Function_&& f, Builders_&&... builders)
	{
		return InvokeWithBuildersImpl<sizeof...(Builders_)>::Apply(std::forward<Function_>(f), std::forward<Builders_>(builders)...);
	}

	struct SelectRed
	{
		template <typename Py_>
		auto operator()(const Py_& p) const
		{
			return p.red;
		}
	};

	struct SelectGreen
	{
		template <typename Py_>
		auto operator()(const Py_& p) const
		{
			return p.green;
		}
	};

	struct SelectBlue
	{
		template <typename Py_>
		auto operator()(const Py_& p) const
		{
			return p.blue;
		}
	};

	struct SelectRgbBrightness
	{
		template <typename Py_>
		auto operator()(const Py_& p) const
		{
			return Brightness{}(p);
		}
	};

	struct SelectReal
	{
		template <typename Py_>
		auto operator()(const Py_& p) const
		{
			return p.r;
		}
	};

	struct SelectImag
	{
		template <typename Py_>
		auto operator()(const Py_& p) const
		{
			return p.i;
		}
	};

	struct SelectAbs
	{
		template <typename Py_>
		auto operator()(const Py_& p) const
		{
			return std::abs(std::complex<ToValueType<Py_>>(p.r, p.i));
		}
	};

	struct SelectIdentity
	{
		template <typename Py_>
		auto operator()(const Py_& p) const
		{
			return p;
		}
	};

	template <typename Ty_>
	void SetIntMinMax(void* outMinVal, void* outMaxVal)
	{
		if (outMinVal) {
			*static_cast<Ty_*>(outMinVal) = std::numeric_limits<Ty_>::lowest();
		}
		if (outMaxVal) {
			*static_cast<Ty_*>(outMaxVal) = std::numeric_limits<Ty_>::max();
		}
	}

	void ClearHistogram(uint32_t* hist, uint32_t stride, uint32_t binsNumber) {
		if (hist) {
			if (stride == 1) {
				std::memset(hist, 0, binsNumber * sizeof(uint32_t));
			}
			else {
				for (uint32_t i = 0; i < binsNumber; ++i, hist += stride) {
					*hist = 0u;
				}
			}
		}
	}

	template <typename PixelType_>
	bool FindHistogramBounds(FIBITMAP* dib, ToValueType<PixelType_>& minVal, ToValueType<PixelType_>& maxVal, void* outMinVal, void* outMaxVal)
	{
		PixelType_ minChannelValues, maxChannelValues;
		if (!FreeImage_FindMinMaxValue(dib, &minChannelValues, &maxChannelValues)) {
			return false;
		}
		minVal = PixelMin(StripAlpha(minChannelValues));
		maxVal = PixelMax(StripAlpha(maxChannelValues));
		if (outMinVal) {
			*static_cast<ToValueType<PixelType_>*>(outMinVal) = minVal;
		}
		if (outMaxVal) {
			*static_cast<ToValueType<PixelType_>*>(outMaxVal) = maxVal;
		}
		return true;
	}

} // namespace


FIBOOL FreeImage_MakeHistogram(FIBITMAP* dib, uint32_t binsNumber, void* outMinVal, void* outMaxVal, uint32_t* histR, uint32_t strideR, uint32_t* histG, uint32_t strideG, uint32_t* histB, uint32_t strideB, uint32_t* histL, uint32_t strideL)
{
	if (!FreeImage_HasPixels(dib) || binsNumber < 1) {
		return FALSE;
	}
	if ((histR && strideR <= 0) || (histG && strideG <= 0) || (histB && strideB <= 0) || (histL && strideL <= 0)) {
		return FALSE;
	}

	if (!histR && !histG && !histB && !histL) {
		return TRUE;
	}

	ClearHistogram(histR, strideR, binsNumber);
	ClearHistogram(histG, strideG, binsNumber);
	ClearHistogram(histB, strideB, binsNumber);
	ClearHistogram(histL, strideL, binsNumber);

	bool success = FALSE;
	switch (FreeImage_GetImageType(dib)) {
	case FIT_BITMAP: {
			const auto bpp = FreeImage_GetBPP(dib);
			const auto colorType = FreeImage_GetColorType2(dib);
			if ((colorType == FIC_RGBALPHA || colorType == FIC_YUV) && (bpp == 32)) {
				success = InvokeWithBuilders(HistogramUInt<FIRGBA8>(dib, binsNumber), HistogramBuilder<SelectRed>(histR, strideR), HistogramBuilder<SelectGreen>(histG, strideG),
					HistogramBuilder<SelectBlue>(histB, strideB), HistogramBuilder<SelectRgbBrightness>(histL, strideL));
			}
			else if ((colorType == FIC_RGB || colorType == FIC_YUV) && (bpp == 24)) {
				success = InvokeWithBuilders(HistogramUInt<FIRGB8>(dib, binsNumber), HistogramBuilder<SelectRed>(histR, strideR), HistogramBuilder<SelectGreen>(histG, strideG),
					HistogramBuilder<SelectBlue>(histB, strideB), HistogramBuilder<SelectRgbBrightness>(histL, strideL));
			}
			else if (colorType == FIC_MINISBLACK && bpp == 8) {
				success = InvokeWithBuilders(HistogramUInt<uint8_t>(dib, binsNumber), HistogramBuilder<SelectIdentity>(histR, strideR));
			}
			if (success) {
				SetIntMinMax<uint8_t>(outMinVal, outMaxVal);
			}
		}
		break;
	case FIT_RGBF: {
			float minVal{}, maxVal{};
			if (!FindHistogramBounds<FIRGBF>(dib, minVal, maxVal, outMinVal, outMaxVal)) {
				break;
			}
			success = InvokeWithBuilders(HistogramFloat<FIRGBF>(dib, binsNumber, minVal, maxVal), HistogramBuilder<SelectRed>(histR, strideR),
				HistogramBuilder<SelectGreen>(histG, strideG), HistogramBuilder<SelectBlue>(histB, strideB), HistogramBuilder<SelectRgbBrightness>(histL, strideL));
		}
		break;
	case FIT_RGBAF: {
			float minVal{}, maxVal{};
			if (!FindHistogramBounds<FIRGBAF>(dib, minVal, maxVal, outMinVal, outMaxVal)) {
				break;
			}
			success = InvokeWithBuilders(HistogramFloat<FIRGBAF>(dib, binsNumber, minVal, maxVal), HistogramBuilder<SelectRed>(histR, strideR),
				HistogramBuilder<SelectGreen>(histG, strideG), HistogramBuilder<SelectBlue>(histB, strideB), HistogramBuilder<SelectRgbBrightness>(histL, strideL));
		}
		break;
	case FIT_COMPLEX: {
			double minVal{}, maxVal{};
			if (!FindHistogramBounds<FICOMPLEX>(dib, minVal, maxVal, outMinVal, outMaxVal)) {
				break;
			}
			success = InvokeWithBuilders(HistogramFloat<FICOMPLEX>(dib, binsNumber, minVal, maxVal), HistogramBuilder<SelectReal>(histR, strideR),
				HistogramBuilder<SelectImag>(histG, strideG), HistogramBuilder<SelectAbs>(histB, strideB));
		}
		break;
	case FIT_COMPLEXF: {
			float minVal{}, maxVal{};
			if (!FindHistogramBounds<FICOMPLEXF>(dib, minVal, maxVal, outMinVal, outMaxVal)) {
				break;
			}
			success = InvokeWithBuilders(HistogramFloat<FICOMPLEXF>(dib, binsNumber, minVal, maxVal), HistogramBuilder<SelectReal>(histR, strideR),
				HistogramBuilder<SelectImag>(histG, strideG), HistogramBuilder<SelectAbs>(histB, strideB));
		}
		break;
	case FIT_DOUBLE: {
			double minVal{}, maxVal{};
			if (!FindHistogramBounds<double>(dib, minVal, maxVal, outMinVal, outMaxVal)) {
				break;
			}
			success = InvokeWithBuilders(HistogramFloat<double>(dib, binsNumber, minVal, maxVal), HistogramBuilder<SelectIdentity>(histR, strideR));
		}
		break;
	case FIT_FLOAT: {
			float minVal{}, maxVal{};
			if (!FindHistogramBounds<float>(dib, minVal, maxVal, outMinVal, outMaxVal)) {
				break;
			}
			success = InvokeWithBuilders(HistogramFloat<float>(dib, binsNumber, minVal, maxVal), HistogramBuilder<SelectIdentity>(histR, strideR));
		}
		break;
	case FIT_RGBA32:
		success = InvokeWithBuilders(HistogramUInt<FIRGBA32>(dib, binsNumber), HistogramBuilder<SelectRed>(histR, strideR),
			HistogramBuilder<SelectGreen>(histG, strideG), HistogramBuilder<SelectBlue>(histB, strideB), HistogramBuilder<SelectRgbBrightness>(histL, strideL));
		if (success) {
			SetIntMinMax<uint32_t>(outMinVal, outMaxVal);
		}
		break;
	case FIT_RGB32:
		success = InvokeWithBuilders(HistogramUInt<FIRGB32>(dib, binsNumber), HistogramBuilder<SelectRed>(histR, strideR),
			HistogramBuilder<SelectGreen>(histG, strideG), HistogramBuilder<SelectBlue>(histB, strideB), HistogramBuilder<SelectRgbBrightness>(histL, strideL));
		if (success) {
			SetIntMinMax<uint32_t>(outMinVal, outMaxVal);
		}
		break;
	case FIT_RGBA16:
		success = InvokeWithBuilders(HistogramUInt<FIRGBA16>(dib, binsNumber), HistogramBuilder<SelectRed>(histR, strideR),
			HistogramBuilder<SelectGreen>(histG, strideG), HistogramBuilder<SelectBlue>(histB, strideB), HistogramBuilder<SelectRgbBrightness>(histL, strideL));
		if (success) {
			SetIntMinMax<uint16_t>(outMinVal, outMaxVal);
		}
		break;
	case FIT_RGB16:
		success = InvokeWithBuilders(HistogramUInt<FIRGB16>(dib, binsNumber), HistogramBuilder<SelectRed>(histR, strideR),
			HistogramBuilder<SelectGreen>(histG, strideG), HistogramBuilder<SelectBlue>(histB, strideB), HistogramBuilder<SelectRgbBrightness>(histL, strideL));
		if (success) {
			SetIntMinMax<uint16_t>(outMinVal, outMaxVal);
		}
		break;
	case FIT_UINT32:
		success = InvokeWithBuilders(HistogramUInt<uint32_t>(dib, binsNumber), HistogramBuilder<SelectIdentity>(histR, strideR));
		if (success) {
			SetIntMinMax<uint32_t>(outMinVal, outMaxVal);
		}
		break;
	case FIT_INT32:
		success = InvokeWithBuilders(HistogramSInt<int32_t>(dib, binsNumber), HistogramBuilder<SelectIdentity>(histR, strideR));
		if (success) {
			SetIntMinMax<int32_t>(outMinVal, outMaxVal);
		}
		break;
	case FIT_UINT16:
		success = InvokeWithBuilders(HistogramUInt<uint16_t>(dib, binsNumber), HistogramBuilder<SelectIdentity>(histR, strideR));
		if (success) {
			SetIntMinMax<uint16_t>(outMinVal, outMaxVal);
		}
		break;
	case FIT_INT16:
		success = InvokeWithBuilders(HistogramSInt<int16_t>(dib, binsNumber), HistogramBuilder<SelectIdentity>(histR, strideR));
		if (success) {
			SetIntMinMax<int16_t>(outMinVal, outMaxVal);
		}
		break;
	default:
		return FALSE;
	}

	return success ? TRUE : FALSE;
}


// ----------------------------------------------------------


/** @brief Creates a lookup table to be used with FreeImage_AdjustCurve() which
 may adjust brightness and contrast, correct gamma and invert the image with a
 single call to FreeImage_AdjustCurve().
 
 This function creates a lookup table to be used with FreeImage_AdjustCurve()
 which may adjust brightness and contrast, correct gamma and invert the image
 with a single call to FreeImage_AdjustCurve(). If more than one of these image
 display properties need to be adjusted, using a combined lookup table should be
 preferred over calling each adjustment function separately. That's particularly
 true for huge images or if performance is an issue. Then, the expensive process
 of iterating over all pixels of an image is performed only once and not up to
 four times.
 
 Furthermore, the lookup table created does not depend on the order, in which
 each single adjustment operation is performed. Due to rounding and byte casting
 issues, it actually matters in which order individual adjustment operations
 are performed. Both of the following snippets most likely produce different
 results:
 
 // snippet 1: contrast, brightness
 FreeImage_AdjustContrast(dib, 15.0);
 FreeImage_AdjustBrightness(dib, 50.0); 
 
 // snippet 2: brightness, contrast
 FreeImage_AdjustBrightness(dib, 50.0);
 FreeImage_AdjustContrast(dib, 15.0);
 
 Better and even faster would be snippet 3:
 
 // snippet 3:
 uint8_t LUT[256];
 FreeImage_GetAdjustColorsLookupTable(LUT, 50.0, 15.0, 1.0, FALSE); 
 FreeImage_AdjustCurve(dib, LUT, FICC_RGB);
 
 This function is also used internally by FreeImage_AdjustColors(), which does
 not return the lookup table, but uses it to call FreeImage_AdjustCurve() on the
 passed image.
 
 @param LUT Output lookup table to be used with FreeImage_AdjustCurve(). <b>The
 size of 'LUT' is assumed to be 256.</b>
 @param brightness Percentage brightness value where -100 <= brightness <= 100<br>
 A value of 0 means no change, less than 0 will make the image darker and greater
 than 0 will make the image brighter.
 @param contrast Percentage contrast value where -100 <= contrast <= 100<br>
 A value of 0 means no change, less than 0 will decrease the contrast
 and greater than 0 will increase the contrast of the image.
 @param gamma Gamma value to be used for gamma correction. A value of 1.0 leaves
 the image alone, less than one darkens it, and greater than one lightens it.
 This parameter must not be zero or smaller than zero. If so, it will be ignored
 and no gamma correction will be performed using the lookup table created.
 @param invert If set to TRUE, the image will be inverted.
 @return Returns the number of adjustments applied to the resulting lookup table
 compared to a blind lookup table.
 */
int DLL_CALLCONV
FreeImage_GetAdjustColorsLookupTable(uint8_t *LUT, double brightness, double contrast, double gamma, FIBOOL invert) {
	double dblLUT[256];
	double value;
	int result = 0;

	if ((brightness == 0.0) && (contrast == 0.0) && (gamma == 1.0) && (!invert)) {
		// nothing to do, if all arguments have their default values
		// return a blind LUT
		for (int i = 0; i < 256; i++) {
			LUT[i] = (uint8_t)i;
		}
		return 0;
	}

	// first, create a blind LUT, which does nothing to the image
	for (int i = 0; i < 256; i++) {
		dblLUT[i] = i;
	}

	if (contrast != 0.0) {
		// modify lookup table with contrast adjustment data
		const double v = (100.0 + contrast) / 100.0;
		for (int i = 0; i < 256; i++) {
			value = 128 + (dblLUT[i] - 128) * v;
			dblLUT[i] = MAX(0.0, MIN(value, 255.0));
		}
		result++;
	}

	if (brightness != 0.0) {
		// modify lookup table with brightness adjustment data
		const double v = (100.0 + brightness) / 100.0;
		for (int i = 0; i < 256; i++) {
			value = dblLUT[i] * v;
			dblLUT[i] = MAX(0.0, MIN(value, 255.0));
		}
		result++;
	}

	if ((gamma > 0) && (gamma != 1.0)) {
		// modify lookup table with gamma adjustment data
		double exponent = 1 / gamma;
		const double v = 255.0 * (double)pow((double)255, -exponent);
		for (int i = 0; i < 256; i++) {
			value = pow(dblLUT[i], exponent) * v;
			dblLUT[i] = MAX(0.0, MIN(value, 255.0));
		}
		result++;
	}

	if (!invert) {
		for (int i = 0; i < 256; i++) {
			LUT[i] = (uint8_t)floor(dblLUT[i] + 0.5);
		}
	} else {
		for (int i = 0; i < 256; i++) {
			LUT[i] = 255 - (uint8_t)floor(dblLUT[i] + 0.5);
		}
		result++;
	}
	// return the number of adjustments made
	return result;
}

/** @brief Adjusts an image's brightness, contrast and gamma as well as it may
 optionally invert the image within a single operation.
 
 This function adjusts an image's brightness, contrast and gamma as well as it
 may optionally invert the image within a single operation. If more than one of
 these image display properties need to be adjusted, using this function should
 be preferred over calling each adjustment function separately. That's
 particularly true for huge images or if performance is an issue.
 
 This function relies on FreeImage_GetAdjustColorsLookupTable(), which creates a
 single lookup table, that combines all adjustment operations requested.
 
 Furthermore, the lookup table created by FreeImage_GetAdjustColorsLookupTable()
 does not depend on the order, in which each single adjustment operation is
 performed. Due to rounding and byte casting issues, it actually matters in which
 order individual adjustment operations are performed. Both of the following
 snippets most likely produce different results:
 
 // snippet 1: contrast, brightness
 FreeImage_AdjustContrast(dib, 15.0);
 FreeImage_AdjustBrightness(dib, 50.0); 
 
 // snippet 2: brightness, contrast
 FreeImage_AdjustBrightness(dib, 50.0);
 FreeImage_AdjustContrast(dib, 15.0);
 
 Better and even faster would be snippet 3:
 
 // snippet 3:
 FreeImage_AdjustColors(dib, 50.0, 15.0, 1.0, FALSE);
 
 @param dib Input/output image to be processed.
 @param brightness Percentage brightness value where -100 <= brightness <= 100<br>
 A value of 0 means no change, less than 0 will make the image darker and greater
 than 0 will make the image brighter.
 @param contrast Percentage contrast value where -100 <= contrast <= 100<br>
 A value of 0 means no change, less than 0 will decrease the contrast
 and greater than 0 will increase the contrast of the image.
 @param gamma Gamma value to be used for gamma correction. A value of 1.0 leaves
 the image alone, less than one darkens it, and greater than one lightens it.<br>
 This parameter must not be zero or smaller than zero. If so, it will be ignored
 and no gamma correction will be performed on the image.
 @param invert If set to TRUE, the image will be inverted.
 @return Returns TRUE on success, FALSE otherwise (e.g. when the bitdeph of the
 source dib cannot be handled).
 */
FIBOOL DLL_CALLCONV
FreeImage_AdjustColors(FIBITMAP *dib, double brightness, double contrast, double gamma, FIBOOL invert) {
	uint8_t LUT[256];

	if (!FreeImage_HasPixels(dib) || (FreeImage_GetImageType(dib) != FIT_BITMAP)) {
		return FALSE;
	}

	int bpp = FreeImage_GetBPP(dib);
	if ((bpp != 8) && (bpp != 24) && (bpp != 32)) {
		return FALSE;
	}

	if (FreeImage_GetAdjustColorsLookupTable(LUT, brightness, contrast, gamma, invert)) {
		return FreeImage_AdjustCurve(dib, LUT, FICC_RGB);
	}
	return FALSE;
}

/** @brief Applies color mapping for one or several colors on a 1-, 4- or 8-bit
 palletized or a 16-, 24- or 32-bit high color image.

 This function maps up to <i>count</i> colors specified in <i>srccolors</i> to
 these specified in <i>dstcolors</i>. Thereby, color <i>srccolors[N]</i>,
 if found in the image, will be replaced by color <i>dstcolors[N]</i>. If
 parameter <i>swap</i> is TRUE, additionally all colors specified in
 <i>dstcolors</i> are also mapped to these specified in <i>srccolors</i>. For
 high color images, the actual image data will be modified whereas, for
 palletized images only the palette will be changed.<br>

 The function returns the number of pixels changed or zero, if no pixels were
 changed. 

 Both arrays <i>srccolors</i> and <i>dstcolors</i> are assumed not to hold less
 than <i>count</i> colors.<br>

 For 16-bit images, all colors specified are transparently converted to their 
 proper 16-bit representation (either in RGB555 or RGB565 format, which is
 determined by the image's red- green- and blue-mask).<br>

 <b>Note, that this behaviour is different from what FreeImage_ApplyPaletteIndexMapping()
 does, which modifies the actual image data on palletized images.</b>

 @param dib Input/output image to be processed.
 @param srccolors Array of colors to be used as the mapping source.
 @param dstcolors Array of colors to be used as the mapping destination.
 @param count The number of colors to be mapped. This is the size of both
 <i>srccolors</i> and <i>dstcolors</i>.  
 @param ignore_alpha If TRUE, 32-bit images and colors are treated as 24-bit.
 @param swap If TRUE, source and destination colors are swapped, that is,
 each destination color is also mapped to the corresponding source color.  
 @return Returns the total number of pixels changed. 
 */
unsigned DLL_CALLCONV
FreeImage_ApplyColorMapping(FIBITMAP *dib, FIRGBA8 *srccolors, FIRGBA8 *dstcolors, unsigned count, FIBOOL ignore_alpha, FIBOOL swap) {
	unsigned result = 0;

	if (!FreeImage_HasPixels(dib) || (FreeImage_GetImageType(dib) != FIT_BITMAP)) {
		return 0;
	}

	// validate parameters
	if ((!srccolors) || (!dstcolors)|| (count < 1)) {
		return 0;
	}

	int bpp = FreeImage_GetBPP(dib);
	switch (bpp) {
		case 1:
		case 4:
		case 8: {
			unsigned size = FreeImage_GetColorsUsed(dib);
			FIRGBA8 *pal = FreeImage_GetPalette(dib);
			const FIRGBA8 *a, *b;
			for (unsigned x = 0; x < size; x++) {
				for (unsigned j = 0; j < count; j++) {
					a = srccolors;
					b = dstcolors;
					for (int i = (swap ? 0 : 1); i < 2; i++) {
						if ((pal[x].blue == a[j].blue)&&(pal[x].green == a[j].green) &&(pal[x].red== a[j].red)) {
							pal[x].blue = b[j].blue;
							pal[x].green = b[j].green;
							pal[x].red = b[j].red;
							result++;
							j = count;
							break;
						}
						a = dstcolors;
						b = srccolors;
					}
				}
			}
			return result;
		}
		case 16: {
			auto *src16 = (uint16_t *)malloc(sizeof(uint16_t) * count);
			if (!src16) {
				return 0;
			}

			auto *dst16 = (uint16_t *)malloc(sizeof(uint16_t) * count);
			if (!dst16) {
				free(src16);
				return 0;
			}

			for (unsigned j = 0; j < count; j++) {
				src16[j] = RGBQUAD_TO_WORD(dib, (srccolors + j));
				dst16[j] = RGBQUAD_TO_WORD(dib, (dstcolors + j));
			}

			const unsigned height = FreeImage_GetHeight(dib);
			const unsigned width = FreeImage_GetWidth(dib);
			const uint16_t *a, *b;
			for (unsigned y = 0; y < height; y++) {
				uint16_t *bits = (uint16_t *)FreeImage_GetScanLine(dib, y);
				for (unsigned x = 0; x < width; x++, bits++) {
					for (unsigned j = 0; j < count; j++) {
						a = src16;
						b = dst16;
						for (int i = (swap ? 0 : 1); i < 2; i++) {
							if (*bits == a[j]) {
								*bits = b[j];
								result++;
								j = count;
								break;
							}
							a = dst16;
							b = src16;
						}
					}
				}
			}
			free(src16);
			free(dst16);
			return result;
		}
		case 24: {
			const unsigned height = FreeImage_GetHeight(dib);
			const unsigned width = FreeImage_GetWidth(dib);
			const FIRGBA8 *a, *b;
			for (unsigned y = 0; y < height; y++) {
				uint8_t *bits = FreeImage_GetScanLine(dib, y);
				for (unsigned x = 0; x < width; x++, bits += 3) {
					for (unsigned j = 0; j < count; j++) {
						a = srccolors;
						b = dstcolors;
						for (int i = (swap ? 0 : 1); i < 2; i++) {
							if ((bits[FI_RGBA_BLUE] == a[j].blue) && (bits[FI_RGBA_GREEN] == a[j].green) &&(bits[FI_RGBA_RED] == a[j].red)) {
								bits[FI_RGBA_BLUE] = b[j].blue;
								bits[FI_RGBA_GREEN] = b[j].green;
								bits[FI_RGBA_RED] = b[j].red;
								result++;
								j = count;
								break;
							}
							a = dstcolors;
							b = srccolors;
						}
					}
				}
			}
			return result;
		}
		case 32: {
			const unsigned height = FreeImage_GetHeight(dib);
			const unsigned width = FreeImage_GetWidth(dib);
			const FIRGBA8 *a, *b;
			for (unsigned y = 0; y < height; y++) {
				uint8_t *bits = FreeImage_GetScanLine(dib, y);
				for (unsigned x = 0; x < width; x++, bits += 4) {
					for (unsigned j = 0; j < count; j++) {
						a = srccolors;
						b = dstcolors;
						for (int i = (swap ? 0 : 1); i < 2; i++) {
							if ((bits[FI_RGBA_BLUE] == a[j].blue) &&(bits[FI_RGBA_GREEN] == a[j].green) &&(bits[FI_RGBA_RED] == a[j].red)
								&&((ignore_alpha) || (bits[FI_RGBA_ALPHA] == a[j].alpha))) {
								bits[FI_RGBA_BLUE] = b[j].blue;
								bits[FI_RGBA_GREEN] = b[j].green;
								bits[FI_RGBA_RED] = b[j].red;
								if (!ignore_alpha) {
									bits[FI_RGBA_ALPHA] = b[j].alpha;
								}
								result++;
								j = count;
								break;
							}
							a = dstcolors;
							b = srccolors;
						}
					}
				}
			}
			return result;
		}
		default: {
			return 0;
		}
	}
}

/** @brief Swaps two specified colors on a 1-, 4- or 8-bit palletized
 or a 16-, 24- or 32-bit high color image.

 This function swaps the two specified colors <i>color_a</i> and <i>color_b</i>
 on a palletized or high color image. For high color images, the actual image
 data will be modified whereas, for palletized images only the palette will be
 changed.<br>

 <b>Note, that this behaviour is different from what FreeImage_SwapPaletteIndices()
 does, which modifies the actual image data on palletized images.</b><br>

 This is just a thin wrapper for FreeImage_ApplyColorMapping() and resolves to:<br>
 <i>return FreeImage_ApplyColorMapping(dib, color_a, color_b, 1, ignore_alpha, TRUE);</i>

 @param dib Input/output image to be processed.
 @param color_a On of the two colors to be swapped.
 @param color_b The other of the two colors to be swapped.
 @param ignore_alpha If TRUE, 32-bit images and colors are treated as 24-bit. 
 @return Returns the total number of pixels changed. 
 */
unsigned DLL_CALLCONV
FreeImage_SwapColors(FIBITMAP *dib, FIRGBA8 *color_a, FIRGBA8 *color_b, FIBOOL ignore_alpha) {
	return FreeImage_ApplyColorMapping(dib, color_a, color_b, 1, ignore_alpha, TRUE);
}

/** @brief Applies palette index mapping for one or several indices on a 1-, 4- 
 or 8-bit palletized image.

 This function maps up to <i>count</i> palette indices specified in
 <i>srcindices</i> to these specified in <i>dstindices</i>. Thereby, index 
 <i>srcindices[N]</i>, if present in the image, will be replaced by index
 <i>dstindices[N]</i>. If parameter <i>swap</i> is TRUE, additionally all indices
 specified in <i>dstindices</i> are also mapped to these specified in 
 <i>srcindices</i>.<br>

 The function returns the number of pixels changed or zero, if no pixels were
 changed. 

 Both arrays <i>srcindices</i> and <i>dstindices</i> are assumed not to hold less
 than <i>count</i> indices.<br>

 <b>Note, that this behaviour is different from what FreeImage_ApplyColorMapping()
 does, which modifies the actual image data on palletized images.</b>

 @param dib Input/output image to be processed.
 @param srcindices Array of palette indices to be used as the mapping source.
 @param dstindices Array of palette indices to be used as the mapping destination.
 @param count The number of palette indices to be mapped. This is the size of both
 <i>srcindices</i> and <i>dstindices</i>.  
 @param swap If TRUE, source and destination palette indices are swapped, that is,
 each destination index is also mapped to the corresponding source index.  
 @return Returns the total number of pixels changed. 
 */
unsigned DLL_CALLCONV
FreeImage_ApplyPaletteIndexMapping(FIBITMAP *dib, uint8_t *srcindices,	uint8_t *dstindices, unsigned count, FIBOOL swap) {
	unsigned result = 0;

	if (!FreeImage_HasPixels(dib) || (FreeImage_GetImageType(dib) != FIT_BITMAP)) {
		return 0;
	}

	// validate parameters
	if ((!srcindices) || (!dstindices)|| (count < 1)) {
		return 0;
	}

	const unsigned height = FreeImage_GetHeight(dib);
	const unsigned width = FreeImage_GetLine(dib);
	const uint8_t *a, *b;

	int bpp = FreeImage_GetBPP(dib);
	switch (bpp) {
		case 1: {

			return result;
		}
		case 4: {
			const int skip_last = (FreeImage_GetWidth(dib) & 0x01);
			unsigned max_x = width - 1;
			for (unsigned y = 0; y < height; y++) {
				uint8_t *bits = FreeImage_GetScanLine(dib, y);
				for (unsigned x = 0; x < width; x++) {
					int start = ((skip_last) && (x == max_x)) ? 1 : 0;
					for (int cn = start; cn < 2; cn++) {
						for (unsigned j = 0; j < count; j++) {
							a = srcindices;
							b = dstindices;
							for (int i = ((swap) ? 0 : 1); i < 2; i++) {
								if (GET_NIBBLE(cn, bits[x]) == (a[j] & 0x0F)) {
									SET_NIBBLE(cn, bits[x], b[j]);
									result++;
									j = count;
									break;
								}
								a = dstindices;
								b = srcindices;
							}
						}
					}
				}
			}
			return result;
		}
		case 8: {
			for (unsigned y = 0; y < height; y++) {
				uint8_t *bits = FreeImage_GetScanLine(dib, y);
				for (unsigned x = 0; x < width; x++) {
					for (unsigned j = 0; j < count; j++) {
						a = srcindices;
						b = dstindices;
						for (int i = ((swap) ? 0 : 1); i < 2; i++) {
							if (bits[x] == a[j]) {
								bits[x] = b[j];
								result++;
								j = count;
								break;
							}
							a = dstindices;
							b = srcindices;
						}
					}
				}
			}
			return result;
		}
		default: {
			return 0;
		}
	}
}

/** @brief Swaps two specified palette indices on a 1-, 4- or 8-bit palletized
 image.

 This function swaps the two specified palette indices <i>index_a</i> and
 <i>index_b</i> on a palletized image. Therefore, not the palette, but the
 actual image data will be modified.<br>

 <b>Note, that this behaviour is different from what FreeImage_SwapColors() does
 on palletized images, which only swaps the colors in the palette.</b><br>

 This is just a thin wrapper for FreeImage_ApplyColorMapping() and resolves to:<br>
 <i>return FreeImage_ApplyPaletteIndexMapping(dib, index_a, index_b, 1, TRUE);</i>

 @param dib Input/output image to be processed.
 @param index_a On of the two palette indices to be swapped.
 @param index_b The other of the two palette indices to be swapped.
 @return Returns the total number of pixels changed. 
 */
unsigned DLL_CALLCONV 
FreeImage_SwapPaletteIndices(FIBITMAP *dib, uint8_t *index_a, uint8_t *index_b) {
	return FreeImage_ApplyPaletteIndexMapping(dib, index_a, index_b, 1, TRUE);
}



namespace
{
	template <typename DstTy_, typename SrcTy_>
	FIBOOL StaticCastPixelValue(void* dst_pixel, const SrcTy_* src_pixel)
	{
		*static_cast<DstTy_*>(dst_pixel) = static_cast<DstTy_>(*src_pixel);
		return TRUE;
	}

	template <typename SrcTy_>
	FIBOOL CastPixelValueImpl(const void* src_pixel, FREE_IMAGE_TYPE dst_type, void* dst_pixel)
	{
		const SrcTy_* src = static_cast<const SrcTy_*>(src_pixel);
		switch (dst_type) {
		case FIT_COMPLEX:
		case FIT_DOUBLE:
			return StaticCastPixelValue<double>(dst_pixel, src);
		case FIT_FLOAT:
		case FIT_COMPLEXF:
		case FIT_RGBAF:
		case FIT_RGBF:
			return StaticCastPixelValue<float>(dst_pixel, src);
		case FIT_UINT32:
		case FIT_RGBA32:
		case FIT_RGB32:
			return StaticCastPixelValue<uint32_t>(dst_pixel, src);
		case FIT_INT32:
			return StaticCastPixelValue<int32_t>(dst_pixel, src);
		case FIT_UINT16:
		case FIT_RGBA16:
		case FIT_RGB16:
			return StaticCastPixelValue<uint16_t>(dst_pixel, src);
		case FIT_INT16:
			return StaticCastPixelValue<int16_t>(dst_pixel, src);
			break;
		case FIT_BITMAP:
			return StaticCastPixelValue<uint8_t>(dst_pixel, src);
			break;
		default:
			return FALSE;
		}
	}

} // namespace

FIBOOL CastPixelValue(FREE_IMAGE_TYPE src_type, const void* src_pixel, FREE_IMAGE_TYPE dst_type, void* dst_pixel)
{
	if (!src_pixel || !dst_pixel) {
		return FALSE;
	}
	switch (src_type) {
	case FIT_COMPLEX:
	case FIT_DOUBLE:
		return CastPixelValueImpl<double>(src_pixel, dst_type, dst_pixel);
	case FIT_FLOAT:
	case FIT_COMPLEXF:
	case FIT_RGBAF:
	case FIT_RGBF:
		return CastPixelValueImpl<float>(src_pixel, dst_type, dst_pixel);
	case FIT_UINT32:
	case FIT_RGBA32:
	case FIT_RGB32:
		return CastPixelValueImpl<uint32_t>(src_pixel, dst_type, dst_pixel);
	case FIT_INT32:
		return CastPixelValueImpl<int32_t>(src_pixel, dst_type, dst_pixel);
	case FIT_UINT16:
	case FIT_RGBA16:
	case FIT_RGB16:
		return CastPixelValueImpl<uint16_t>(src_pixel, dst_type, dst_pixel);
	case FIT_INT16:
		return CastPixelValueImpl<int16_t>(src_pixel, dst_type, dst_pixel);
		break;
	case FIT_BITMAP:
		return CastPixelValueImpl<uint8_t>(src_pixel, dst_type, dst_pixel);
		break;
	default:
		return FALSE;
	}
}
