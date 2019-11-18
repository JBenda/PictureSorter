#include "fJPG.hpp"

#include "HuffTable.hpp"
#include "QuadTable.hpp"

#include <istream>

namespace fJPG {

	enum struct Flags : uint8_t {
		DATA
	};
	

	template<ColorEncoding C>
	class EditablePicture : public Picture<C> {
	public:
		EditablePicture(Dim<std::size_t> size, std::size_t channels)
			: Picture<C>(size, channels){}

		auto GetChanel(std::size_t x) {
			return GetChanel(x);
		}
	};

	template <Flags TYPE>
	struct Block {
		static constexpr uint8_t VALUE = static_cast<uint8_t>(TYPE);
		std::streampos start{0};
		std::size_t size{0}; // in byte
	};


	struct ChannelInfo {
		uint8_t id{ 0 }, sampV{ 0 }, sampH{ 0 }, quadTable{ 0 }, huffTableAc{ 0 }, huffTableDc{ 0 };
	}; 
	

	struct JPGDecomposition {
		JPGDecomposition(std::istream& inputStream) : input{inputStream}{}
		static constexpr size_t Max_Channels = 3;
		Dim<std::size_t> size;
		std::size_t nChannel{0};
		std::array<ChannelInfo, Max_Channels> channelInfos;
		std::array<QuadTable, Max_Channels> quadTables;
		std::array<HuffTable, Max_Channels> huffDCTables; ///< drop AC because fast
		Block<Flags::DATA> data;	
		std::istream& input;
	};

	JPGDecomposition Decompose(std::istream&);
	void Decode(EditablePicture<ColorEncoding::YCrCb8>&, const JPGDecomposition&);

	Picture<ColorEncoding::YCrCb8> Convert(std::istream& input) {
		const JPGDecomposition data = Decompose(input);
		
		EditablePicture<ColorEncoding::YCrCb8> 
			picture(data.size, data.nChannel);
		
		Decode(picture, data);

		return std::move(picture);
	}

	JPGDecomposition Decompose(std::istream& input) {
		JPGDecomposition result(input);

		return input;
	}

	void Decode(EditablePicture<ColorEncoding::YCrCb8>& picture, const JPGDecomposition& data) {
	
	}

}
