

// AreaConfiguration creation
CAreaConfiguration *CConfigurableElement::createAreaConfiguration(const CSyncerSet *pSyncerSet) const
{
    return new CAreaConfiguration(this, pSyncerSet);
}



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


// Browse parent path to find syncer
ISyncer *CConfigurableElement::getSyncer() const
{
    // Check parent
    // pParent = CConfigurableDomain
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
    // pParent = CConfigurableDomain 
    return !!pParent->getParent();
}