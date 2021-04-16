std::string CSelectionCriterionLibrary::getKind() const
{
    return "SelectionCriterionLibrary";
}

// Type creation
CSelectionCriterionType *CSelectionCriterionLibrary::createSelectionCriterionType(bool bIsInclusive)
{
    auto pSelectionCriterionType = new CSelectionCriterionType(bIsInclusive);

    addChild(pSelectionCriterionType);

    return pSelectionCriterionType;
}
