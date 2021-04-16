

class CParameterBlackboard : private utility::NonCopyable {

	using Blackboard = std::vector<uint8_t>;
	Blackboard mBlackboard;
};

#define MAKE_ARRAY_ITERATOR(begin, size) begin


Blackboard::const_iterator atOffset(size_t offset) const { return begin(mBlackboard) + offset; }





// Configuration handling
void CParameterBlackboard::restoreFrom(const CParameterBlackboard *pFromBlackboard, size_t offset)
{
    const auto &fromBB = pFromBlackboard->mBlackboard;
    assertValidAccess(offset, fromBB.size());
    std::copy(begin(fromBB), end(fromBB), atOffset(offset));
}





void CParameterBlackboard::writeBuffer(const void *pvSrcData, size_t size, size_t offset)
{
    writeInteger(pvSrcData, size, offset);
}

// Single parameter access
void CParameterBlackboard::writeInteger(const void *pvSrcData, size_t size, size_t offset)
{
    assertValidAccess(offset, size);

    auto first = MAKE_ARRAY_ITERATOR(static_cast<const uint8_t *>(pvSrcData), size);
    auto last = first + size;
    auto dest_first = atOffset(offset);

    std::copy(first, last, dest_first);
}


void CParameterBlackboard::readBuffer(void *pvDstData, size_t size, size_t offset) const
{
    readInteger(pvDstData, size, offset);
}


void CParameterBlackboard::readInteger(void *pvDstData, size_t size, size_t offset) const
{
    assertValidAccess(offset, size);

    auto first = atOffset(offset);
    auto last = first + size;
    auto dest_first = MAKE_ARRAY_ITERATOR(static_cast<uint8_t *>(pvDstData), size);

    std::copy(first, last, dest_first);
}

void CParameterBlackboard::assertValidAccess(size_t offset, size_t size) const
{
    ALWAYS_ASSERT(offset + size <= getSize(),
                  "Invalid data size access: offset=" << offset << " size=" << size
                                                      << "reference size=" << getSize());
}

// Size
void CParameterBlackboard::setSize(size_t size)
{
    mBlackboard.resize(size);
}

size_t CParameterBlackboard::getSize() const
{
    return mBlackboard.size();
}


