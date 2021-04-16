

//  CSelectionCriteria 继承 CElement ， addChild为从父类继承的函数
CSelectionCriteria::CSelectionCriteria()
{
    addChild(new CSelectionCriterionLibrary);
    addChild(new CSelectionCriteriaDefinition);
}


// Selection Criteria/Type creation
CSelectionCriterionType *CSelectionCriteria::createSelectionCriterionType(bool bIsInclusive)
{
	// getSelectionCriterionLibrary 从CSelectionCriteria数组中得到 CSelectionCriterionLibrary
	// 
    return getSelectionCriterionLibrary()->createSelectionCriterionType(bIsInclusive);
}

// Children access
CSelectionCriterionLibrary *CSelectionCriteria::getSelectionCriterionLibrary()
{
    return static_cast<CSelectionCriterionLibrary *>(getChild(ESelectionCriterionLibrary));
}



CSelectionCriterion *CSelectionCriteria::createSelectionCriterion(
    const std::string &strName, const CSelectionCriterionType *pType, core::log::Logger &logger)
{
	// getSelectionCriteriaDefinition 从CSelectionCriteria 的数组中得到 CSelectionCriteriaDefinition
    return getSelectionCriteriaDefinition()->createSelectionCriterion(strName, pType, logger);
}

CSelectionCriteriaDefinition *CSelectionCriteria::getSelectionCriteriaDefinition()
{
    return static_cast<CSelectionCriteriaDefinition *>(getChild(ESelectionCriteriaDefinition));
}