//===========================================================
// FreeImage Re(surrected)
// Modified fork from the original FreeImage 3.18
// with updated dependencies and extended features.
//===========================================================

#ifndef FREEIMAGE_SIMPLE_TOOLS_H_
#define FREEIMAGE_SIMPLE_TOOLS_H_

#include "ConversionYUV.h"
#include <cmath>
#include <tuple>
#include <memory>


using UniqueBitmap = std::unique_ptr<FIBITMAP, decltype(&::FreeImage_Unload)>;


template <typename DstPixel_, typename SrcPixel_ = DstPixel_, typename PixelVisitor_>
void BitmapForEach(FIBITMAP* src, PixelVisitor_ vis)
{
    const unsigned width = FreeImage_GetWidth(src);
    const unsigned height = FreeImage_GetHeight(src);
    const unsigned src_pitch = FreeImage_GetPitch(src);

    const uint8_t* src_bits = FreeImage_GetBits(src);
    for (unsigned y = 0; y < height; ++y) {
        auto src_pixel = static_cast<const SrcPixel_*>(static_cast<const void*>(src_bits));
        for (unsigned x = 0; x < width; ++x) {
            vis(src_pixel[x], x, y);
        }
        src_bits += src_pitch;
    }
}

template <typename DstPixel_, typename SrcPixel_ = DstPixel_, typename UnaryOperation_>
void BitmapTransform(FIBITMAP* dst, FIBITMAP* src, UnaryOperation_ unary_op)
{
	const unsigned width = FreeImage_GetWidth(src);
	const unsigned height = FreeImage_GetHeight(src);
	const unsigned src_pitch = FreeImage_GetPitch(src);
	const unsigned dst_pitch = FreeImage_GetPitch(dst);

	const uint8_t* src_bits = FreeImage_GetBits(src);
	uint8_t* dst_bits = FreeImage_GetBits(dst);

	for (unsigned y = 0; y < height; ++y) {
		auto src_pixel = static_cast<const SrcPixel_*>(static_cast<const void*>(src_bits));
		auto dst_pixel = static_cast<DstPixel_*>(static_cast<void*>(dst_bits));
		for (unsigned x = 0; x < width; ++x) {
			dst_pixel[x] = unary_op(src_pixel[x]);
		}
		src_bits += src_pitch;
		dst_bits += dst_pitch;
	}
}

template <typename Ty_>
using IsIntPixelType = std::integral_constant<bool,
    std::is_same_v<Ty_, FIRGB8> ||
    std::is_same_v<Ty_, FIRGBA8> ||
    std::is_same_v<Ty_, FIRGB16> ||
    std::is_same_v<Ty_, FIRGBA16> ||
    std::is_same_v<Ty_, FIRGB32> ||
    std::is_same_v<Ty_, FIRGBA32>
>;

template <typename Ty_>
using IsFloatPixelType = std::integral_constant<bool,
    std::is_same_v<Ty_, FIRGBF> ||
    std::is_same_v<Ty_, FIRGBAF>
>;

template <typename Ty_>
using IsPixelType = std::integral_constant<bool, IsIntPixelType<Ty_>::value || IsFloatPixelType<Ty_>::value>;

namespace details
{
    template <typename PixelType_>
    struct ToValueTypeImpl {};

    template <> struct ToValueTypeImpl<FIRGBAF>    { using type = float;    };
    template <> struct ToValueTypeImpl<FIRGBF>     { using type = float;    };
    template <> struct ToValueTypeImpl<FIRGBA32>   { using type = uint32_t; };
    template <> struct ToValueTypeImpl<FIRGB32>    { using type = uint32_t; };
    template <> struct ToValueTypeImpl<FIRGBA16>   { using type = uint16_t; };
    template <> struct ToValueTypeImpl<FIRGB16>    { using type = uint16_t; };
    template <> struct ToValueTypeImpl<FIRGBA8>    { using type = uint8_t;  };
    template <> struct ToValueTypeImpl<FIRGB8>     { using type = uint8_t;  };
    template <> struct ToValueTypeImpl<FICOMPLEX>  { using type = double;   };
    template <> struct ToValueTypeImpl<FICOMPLEXF> { using type = float;    };
    template <> struct ToValueTypeImpl<double>     { using type = double;   };
    template <> struct ToValueTypeImpl<float>      { using type = float;    };
    template <> struct ToValueTypeImpl<uint32_t>   { using type = uint32_t; };
    template <> struct ToValueTypeImpl<int32_t>    { using type = int32_t;  };
    template <> struct ToValueTypeImpl<uint16_t>   { using type = uint16_t; };
    template <> struct ToValueTypeImpl<int16_t>    { using type = int16_t;  };
    template <> struct ToValueTypeImpl<uint8_t>    { using type = uint8_t;  };
}

template <typename PixelType_>
using ToValueType = typename details::ToValueTypeImpl<PixelType_>::type;

template <uint32_t Value_>
using uint32_constant = std::integral_constant<uint32_t, Value_>;

template <typename PixelType_>
struct PixelChannelsNumber : public uint32_constant<1>{};

template <> struct PixelChannelsNumber<FIRGBAF>  : public uint32_constant<4> {};
template <> struct PixelChannelsNumber<FIRGBF>   : public uint32_constant<3> {};
template <> struct PixelChannelsNumber<FIRGBA32> : public uint32_constant<4> {};
template <> struct PixelChannelsNumber<FIRGB32>  : public uint32_constant<3> {};
template <> struct PixelChannelsNumber<FIRGBA16> : public uint32_constant<4> {};
template <> struct PixelChannelsNumber<FIRGB16>  : public uint32_constant<3> {};
template <> struct PixelChannelsNumber<FIRGBA8>  : public uint32_constant<4> {};
template <> struct PixelChannelsNumber<FIRGB8>   : public uint32_constant<3> {};
template <> struct PixelChannelsNumber<FICOMPLEX> : public uint32_constant<2> {};
template <> struct PixelChannelsNumber<FICOMPLEXF> : public uint32_constant<2> {};

template <uint32_t ChannelIndex_, typename PixelType_>
inline constexpr
void SetChannel(PixelType_& p, ToValueType<PixelType_> v)
{
    if constexpr (ChannelIndex_ < PixelChannelsNumber<PixelType_>::value) {
        static_cast<ToValueType<PixelType_>*>(static_cast<void*>(&p))[ChannelIndex_] = std::move(v);
    }
}

template <uint32_t ChannelIndex_, typename PixelType_>
inline constexpr
ToValueType<PixelType_> GetChannel(const PixelType_& p, ToValueType<PixelType_> v = static_cast<ToValueType<PixelType_>>(0))
{
    if constexpr (ChannelIndex_ < PixelChannelsNumber<PixelType_>::value) {
        return static_cast<const ToValueType<PixelType_>*>(static_cast<const void*>(&p))[ChannelIndex_];
    }
    else {
        return v;
    }
}

inline
bool IsNan(float p) {
    return std::isnan(p);
}

inline
bool IsNan(double p) {
    return std::isnan(p);
}

inline
bool IsNan(FIRGBF p) {
    return std::isnan(p.red) || std::isnan(p.green) || std::isnan(p.blue);
}

inline
bool IsNan(FIRGBAF p) {
    // Ignore alpha channel, since color can be computed without it...
    return std::isnan(p.red) || std::isnan(p.green) || std::isnan(p.blue);
}

template <typename Ty_>
inline constexpr
std::enable_if_t<std::is_integral_v<Ty_> || IsIntPixelType<Ty_>::value, bool> IsNan(Ty_ p)
{
    return false;
}

struct Brightness
{
    template <typename Ty_>
    inline
    auto operator()(const Ty_& p, std::enable_if_t<std::is_floating_point_v<Ty_> || std::is_integral_v<Ty_>, void*> = nullptr)
    {
        return p;
    }

    template <typename Ty_>
    inline
    auto operator()(const Ty_& p, std::enable_if_t<IsPixelType<Ty_>::value, void*> = nullptr)
    {
        return YuvJPEG::Y(p.red, p.green, p.blue);
    }
};

#endif //FREEIMAGE_SIMPLE_TOOLS_H_
