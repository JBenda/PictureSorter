#include "fJPG.hpp"

#include "HuffTable.hpp"

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
		std::streampos start;
		std::size_t size; // in byte
	};


	struct ChannelInfo {
		uint8_t id, sampV, sampH, quadTable, huffTableAc, huffTableDc;
	}; 

	class QuadTable {
	public:
		int dcValue() { return _dc; }
		friend std::istream& operator>>(std::istream& is, QuadTable& q);
	private:
		int _dc{0}; ///< quantization value[0] (for dc value)
	};
	

	struct JPGDecomposition {
		JPGDecomposition(std::istream& inputStream) : input{inputStream}{}
		static constexpr size_t Max_Channels = 3;
		Dim<std::size_t> size;
		std::size_t nChannel;
		std::array<ChannelInfo, Max_Channels> channelInfos;
		std::array<QuadTable, Max_Channels> quadTables;
		std::array<HuffTable, Max_Channels> huffDCTables; ///< drop AC because fast
		Block<Flags::DATA> data;	
		std::istream& input;
	};

	Picture<ColorEncoding::YCrCb8> Convert(std::istream& input) {
		const JPGDecomposition data(input);// = Decompose(input);
		
		EditablePicture<ColorEncoding::YCrCb8> 
			picture(data.size, data.nChannel);
		
		// Decode(picture, data);

		return std::move(picture);
	}

}
