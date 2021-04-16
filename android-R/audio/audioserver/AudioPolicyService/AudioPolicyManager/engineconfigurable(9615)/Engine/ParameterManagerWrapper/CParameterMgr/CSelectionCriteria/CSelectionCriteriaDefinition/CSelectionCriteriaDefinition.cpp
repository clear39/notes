


// Selection Criterion creation
CSelectionCriterion *CSelectionCriteriaDefinition::createSelectionCriterion(
    const std::string &strName, const CSelectionCriterionType *pType, core::log::Logger &logger)
{
    auto pSelectionCriterion = new CSelectionCriterion(strName, pType, logger);

    addChild(pSelectionCriterion);

    return pSelectionCriterion;
}



// Selection Criterion access
const CSelectionCriterion *CSelectionCriteriaDefinition::getSelectionCriterion(
    const std::string &strName) const
{
    return static_cast<const CSelectionCriterion *>(findChild(strName));
}
