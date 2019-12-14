#pragma once

#include <cstddef>
#include <vector>
#include "Collection.hpp"

namespace GPU {
	class Device {
	public:
		std::size_t batchSize() { return 0; }
		std::vector<float> calcSim( const Batch& b1, const Batch& b2 ) const {
			return std::vector<float>();
		}
	};
}
