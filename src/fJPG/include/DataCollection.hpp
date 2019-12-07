#pragma once

#include "HuffTable.hpp"
#include "QuadTable.hpp"
#include "fJPG.hpp"

#include <optional>
#include <array>

namespace fJPG {
	template<ColorEncoding C>
	class EditablePicture : public Picture<C> {
	public:
		EditablePicture( Dim<std::size_t> size, std::size_t channels )
			: Picture<C>( size, channels ) {}

		auto GetData( std::size_t x ) {
			return GetRawChannel( x );
		}
	};


	struct ChannelInfo {
		uint8_t sampV{ 0 }, sampH{ 0 }, quadTable{ 0 }, huffAc{ 0 }, huffDc{ 0 };
	};


	struct JPGDecomposition {
		JPGDecomposition( std::istream& inputStream ) : input{ inputStream } {}
		static constexpr size_t Max_Channels = 3;
		Dim<std::size_t> size;
		Dim<uint8_t> mcuSize;
		Dim<uint8_t> mcuSamp;
		Dim<std::size_t> nDus;
		std::size_t nChannel{ 0 };
		ColorEncoding colorEncoding{ ColorEncoding::YCrCb8 };
		std::array<ChannelInfo, Max_Channels> channelInfos;
		std::array<QuadTable, Max_Channels> quadTables;
		std::array<HuffTable, Max_Channels> huffTablesAc;
		std::array<HuffTable, Max_Channels> huffTablesDc;
		std::streampos startData;
		std::istream& input;
		struct Progressive {
			uint8_t Ss, Se, Ah, Al;
		};
		std::optional<Progressive> progressive;
	};
}