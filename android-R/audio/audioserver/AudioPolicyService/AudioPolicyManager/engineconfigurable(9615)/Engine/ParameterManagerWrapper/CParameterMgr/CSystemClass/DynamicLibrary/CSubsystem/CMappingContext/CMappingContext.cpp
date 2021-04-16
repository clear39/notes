class PARAMETER_EXPORT CMappingContext{
	struct SItem
    {
        const std::string *strKey{nullptr};
        const std::string *strItem{nullptr};
    };

    // using Items = std::vector<SItem>;
    // Items mItems;
	CMappingContext(size_t uiNbItemTypes) : mItems(uiNbItemTypes) {}
};


// Item access
// 
bool CMappingContext::setItem(size_t itemType, const string *pStrKey, const string *pStrItem)
{
    if (iSet(itemType)) {
        // Already set!
        return false;
    }

    // Set item key
    mItems[itemType].strKey = pStrKey;

    // Set item value
    mItems[itemType].strItem = pStrItem;

    return true;
}

bool CMappingContext::iSet(size_t itemType) const
{
    return mItems[itemType].strItem != nullptr;
}