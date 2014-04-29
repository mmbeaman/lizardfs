#pragma once

#include "config.h"

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include "common/chunk_type_with_address.h"
#include "mount/chunk_locator.h"
#include "mount/write_cache_block.h"
#include "mount/write_executor.h"

class ChunkserverStats;
class ChunkConnector;

class ChunkWriter {
public:
	ChunkWriter(ChunkserverStats& stats, ChunkConnector& connector, int dataChainFd);
	ChunkWriter(const ChunkWriter&) = delete;
	~ChunkWriter();
	ChunkWriter& operator=(const ChunkWriter&) = delete;

	/*
	 * Creates connection chain between client and all chunkservers
	 * This method will throw an exception if all the connections can't be established
	 * within the given timeout.
	 */
	void init(WriteChunkLocator* locator, uint32_t msTimeout);

	/*
	 * Returns minimum number of blocks which will be written to chunkservers by
	 * the ChunkWriter if the flush mode is off.
	 */
	uint32_t getMinimumBlockCountWorthWriting();

	/*
	 * Adds a new write operation.
	 */
	void addOperation(WriteCacheBlock&& block);

	/*
	 * Starts these added operations, which are worth starting right now
	 */
	void startNewOperations();

	/*
	 * Processes all started operations for at most specified time (0 - asap)
	 */
	void processOperations(uint32_t msTimeout);

	/*
	 * Returns number of new and pending write operations.
	 */
	uint32_t getUnfinishedOperationsCount();

	/*
	 * Returns number of pending write operations.
	 */
	uint32_t getPendingOperationsCount();

	/*
	 * Tells ChunkWriter that no more operations will be added and it can flush all the data
	 * to chunkservers. No new operations can be started after calling this method.
	 */
	void startFlushMode();

	/*
	 * Tells ChunkWriter, that it may not start any operation, that did't start yet, because writing
	 * this chunk will be finished later and any then data left in the journal will be written.
	 * No new operations can be started after calling this method.
	 */
	void dropNewOperations();

	/*
	 * Closes connection chain, releases all the acquired locks.
	 * This method can be called when all the write operations have been finished.
	 */
	void finish(uint32_t msTimeout);

	/*
	 * Immediately closes write operations and connection chains.
	 */
	void abortOperations();

	/*
	 * Removes writer's journal and returns it
	 */
	std::list<WriteCacheBlock> releaseJournal();

	bool acceptsNewOperations() { return acceptsNewOperations_; }

private:
	typedef uint32_t WriteId;
	typedef uint32_t OperationId;
	typedef std::list<WriteCacheBlock>::iterator JournalPosition;

	class Operation {
	public:
		std::vector<JournalPosition> journalPositions;  // stripe in the written journal
		std::list<WriteCacheBlock> parityBuffers;       // memory for parity blocks
		uint32_t unfinishedWrites;                      // number of write request sent
		uint64_t offsetOfEnd;                           // offset in the file

		Operation();
		Operation(Operation&&) = default;
		Operation(const Operation&) = delete;
		Operation& operator=(const Operation&) = delete;
		Operation& operator=(Operation&&) = default;

		/**
		 * Checks if expansion can be performed for given stripe
		 */
		bool isExpandPossible(ChunkWriter::JournalPosition newPosition, uint32_t stripeSize);
		/*
		 * Tries to add a new journal position to an existing operation.
		 * Returns true if succeeded, false if the operation wasn't modified.
		 */
		void expand(JournalPosition journalPosition);

		/*
		 * Returns true if two operations write the same place in a file.
		 * One of these operations have to be complete, ie. contain a full stripe
		 */
		bool collidesWith(const Operation& operation) const;

		/*
		 * Returns true if the operation is not a partial-stripe write operation
		 * for a given stripe size
		 */
		bool isFullStripe(uint32_t stripeSize) const;
	};

	ChunkserverStats& chunkserverStats_;
	ChunkConnector& connector_;
	WriteChunkLocator* locator_;
	uint32_t idCounter_;
	bool acceptsNewOperations_;
	uint32_t combinedStripeSize_;
	int dataChainFd_;

	std::map<int, std::unique_ptr<WriteExecutor>> executors_;
	std::list<WriteCacheBlock> journal_;
	std::list<Operation> newOperations_;
	std::map<WriteId, OperationId> writeIdToOperationId_;
	std::map<OperationId, Operation> pendingOperations_;

	bool canStartOperation(const Operation& operation);
	void startOperation(Operation&& operation);
	WriteCacheBlock readBlock(uint32_t blockIndex, ChunkType& readFromChunkType);
	void processStatus(const WriteExecutor& executor, const WriteExecutor::Status& status);
	uint32_t allocateId() {
		// we never return id=0 because it's reserved for WRITE_INIT
		idCounter_++;
		return idCounter_;
	}
};
