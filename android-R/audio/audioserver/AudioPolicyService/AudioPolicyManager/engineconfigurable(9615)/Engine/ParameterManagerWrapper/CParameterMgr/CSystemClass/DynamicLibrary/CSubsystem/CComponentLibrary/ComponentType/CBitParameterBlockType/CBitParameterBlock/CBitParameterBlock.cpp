
class CBitParameterBlock : public CInstanceConfigurableElement{
	
};


// Syncer set (me, ascendant or descendant ones)
void CConfigurableElement::fillSyncerSet(CSyncerSet &syncerSet) const
{
    //  Try me or ascendants
    ISyncer *pMineOrAscendantSyncer = getSyncer();

    if (pMineOrAscendantSyncer) {

        // Provide found syncer object
        syncerSet += pMineOrAscendantSyncer;

        // Done
        return;
    }
    // Fetch descendant ones
    fillSyncerSetFromDescendant(syncerSet);
}

// Syncer
ISyncer *CInstanceConfigurableElement::getSyncer() const
{
    if (_pSyncer) {

        return _pSyncer;
    }
    // Check parent
    return base::getSyncer();
}

// Browse parent path to find syncer
ISyncer *CConfigurableElement::getSyncer() const
{
    // Check parent
    const CElement *pParent = getParent();

    if (isOfConfigurableElementType(pParent)) {

        return static_cast<const CConfigurableElement *>(pParent)->getSyncer();
    }
    return nullptr;
}

// Check parent is still of current type (by structure knowledge)
bool CConfigurableElement::isOfConfigurableElementType(const CElement *pParent) const
{
    assert(pParent);

    // Up to system class
    return !!pParent->getParent();
}