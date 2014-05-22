#pragma once

#include "config.h"

#include <cstdint>
#include <sstream>
#include <string>

#include "common/serialization.h"

class ChunkType {
public:
	typedef uint8_t XorLevel;
	typedef uint8_t XorPart;

	static ChunkType getStandardChunkType();
	static ChunkType getXorChunkType(XorLevel level, XorPart part);
	static ChunkType getXorParityChunkType(XorLevel level);

	bool isStandardChunkType() const;
	bool isXorChunkType() const;
	bool isXorParity() const;
	uint8_t chunkTypeId() const;
	XorLevel getXorLevel() const;
	XorPart getXorPart() const;
	static bool validChunkTypeID(uint8_t chunkTypeId);

	bool operator==(const ChunkType& otherChunkType) const {
		return chunkTypeId_ == otherChunkType.chunkTypeId_;
	}

	bool operator<(const ChunkType& otherChunkType) const {
		return chunkTypeId_ < otherChunkType.chunkTypeId_;
	}

	uint32_t getStripeSize() const {
		return isXorChunkType() ? getXorLevel() : 1;
	}

	static uint32_t chunkLengthToChunkTypeLength(ChunkType ct, uint32_t chunkLength) {
		if (ct.isStandardChunkType()) {
			return chunkLength;
		}
		eassert(ct.isXorChunkType());

		uint32_t fullBlocks = chunkLength / (ct.getXorLevel() * MFSBLOCKSIZE);
		uint32_t baseLen = fullBlocks * MFSBLOCKSIZE;
		uint32_t base = fullBlocks * MFSBLOCKSIZE * ct.getXorLevel();
		uint32_t rest = chunkLength - base;

		uint32_t tmp = 0;
		if (!ct.isXorParity()) {
			tmp = ct.getXorPart() - 1;
		}
		int32_t restLen = rest - tmp * MFSBLOCKSIZE;
		if (restLen < 0) {
			restLen = 0;
		} else if (restLen > MFSBLOCKSIZE) {
			restLen = MFSBLOCKSIZE;
		}
		return baseLen + restLen;
	}

	std::string toString() const {
		if (isStandardChunkType()) {
			return std::string("standard");
		} else {
			std::stringstream ss;
			if (isXorParity()) {
				ss << "xor_parity_of_" <<
						static_cast<unsigned>(getXorLevel());
			} else {
				ss << "xor_" << static_cast<unsigned>(getXorPart())
					<< "_of_" << static_cast<unsigned>(getXorLevel());
			}
			return ss.str();
		}
	}

private:
	static const uint8_t kStandardChunkTypeId = 0;
	static const uint8_t kXorParityPart = 0;

	// Just one 8 bytes to save space (this class will be stored in RAM of master)
	uint8_t chunkTypeId_;

	explicit ChunkType(uint8_t chunkType) : chunkTypeId_(chunkType) {
	}

	friend uint32_t serializedSize(const ChunkType&);
	friend void serialize(uint8_t **, const ChunkType&);
	friend void deserialize(const uint8_t**, uint32_t&, ChunkType&);
};

inline uint32_t serializedSize(const ChunkType& chunkType) {
	return serializedSize(chunkType.chunkTypeId_);
}

inline void serialize(uint8_t **destination, const ChunkType& chunkType) {
	serialize(destination, chunkType.chunkTypeId_);
}

inline void deserialize(const uint8_t** source, uint32_t& bytesLeftInBuffer, ChunkType& chunkType) {
	uint8_t chunkTypeId;
	deserialize(source, bytesLeftInBuffer, chunkTypeId);
	if (ChunkType::validChunkTypeID(chunkTypeId)) {
		chunkType.chunkTypeId_ = chunkTypeId;
	} else {
		throw IncorrectDeserializationException(
				"unknown chunk type id: " + std::to_string(chunkTypeId));
	}
}