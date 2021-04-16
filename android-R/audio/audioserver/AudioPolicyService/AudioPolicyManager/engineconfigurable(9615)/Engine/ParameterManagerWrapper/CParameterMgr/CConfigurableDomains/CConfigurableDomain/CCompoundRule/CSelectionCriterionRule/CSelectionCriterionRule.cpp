
class CSelectionCriterionRule : public CRule {

};


const CSelectionCriterionRule::SMatchingRuleDescription
    CSelectionCriterionRule::_astMatchesWhen[CSelectionCriterionRule::ENbMatchesWhen] = {
        {"Is", true}, {"IsNot", true}, {"Includes", false}, {"Excludes", false}
};

//<SelectionCriterionRule SelectionCriterion="AvailableOutputDevices" MatchesWhen="Includes" Value="Speaker"/>
// From IXmlSink
bool CSelectionCriterionRule::fromXml(const CXmlElement &xmlElement,
                                      CXmlSerializingContext &serializingContext)
{
    // Retrieve actual context
    CXmlDomainImportContext &xmlDomainImportContext =
        static_cast<CXmlDomainImportContext &>(serializingContext);

    // Get selection criterion
    string strSelectionCriterion;
    xmlElement.getAttribute("SelectionCriterion", strSelectionCriterion);

    // xmlDomainImportContext.getSelectionCriteriaDefinition() 实在 CParameterMgr::loadSettingsFromConfigFile 进行设置
    // _pSelectionCriterion = CSelectionCriterion
    _pSelectionCriterion =
        xmlDomainImportContext.getSelectionCriteriaDefinition()->getSelectionCriterion(strSelectionCriterion);

    // Check existence
    if (!_pSelectionCriterion) {
        xmlDomainImportContext.setError("Couldn't find selection criterion " +
                                        strSelectionCriterion + " in " + getKind() + " " +
                                        xmlElement.getPath());

        return false;
    }

    // Get MatchesWhen
    string strMatchesWhen;
    xmlElement.getAttribute("MatchesWhen", strMatchesWhen);
    string strError;

    // 
    if (!setMatchesWhen(strMatchesWhen, strError)) {
        xmlDomainImportContext.setError("Wrong MatchesWhen attribute " + strMatchesWhen + " in " +
                                        getKind() + " " + xmlElement.getPath() + ": " + strError);
        return false;
    }

    // Get Value
    string strValue;
    //根据上面的例子得到strValue为"Speaker"
    xmlElement.getAttribute("Value", strValue);

    // _pSelectionCriterion->getCriterionType() 得到 CSelectionCriterionType 
    // 根据上面的例子 得到 <value numerical="2" literal="Speaker"/>,_iMatchValue的值为2
    if (!_pSelectionCriterion->getCriterionType()->getNumericalValue(strValue, _iMatchValue)) {
        xmlDomainImportContext.setError("Wrong Value attribute value " + strValue + " in " + getKind() + " " + xmlElement.getPath());

        return false;
    }

    // Done
    return true;
}

struct SMatchingRuleDescription
{
    const char *pcMatchesWhen;
    bool bExclusiveTypeCompatible;
};


// XML MatchesWhen attribute parsing
bool CSelectionCriterionRule::setMatchesWhen(const string &strMatchesWhen, string &strError)
{
    for (size_t matchesWhen = 0; matchesWhen < ENbMatchesWhen; matchesWhen++) {
        //{"Is", true}, {"IsNot", true}, {"Includes", false}, {"Excludes", false}};
        const SMatchingRuleDescription *pstMatchingRuleDescription = &_astMatchesWhen[matchesWhen];

        if (strMatchesWhen == pstMatchingRuleDescription->pcMatchesWhen) {

            // Found it!

            // Get Type
            // _pSelectionCriterion->getCriterionType() 得到 CSelectionCriterionType 
            const ISelectionCriterionTypeInterface *pSelectionCriterionType =
                _pSelectionCriterion->getCriterionType();

            //如果SelectionCriterionType为 inclusive，
            //pstMatchingRuleDescription->bExclusiveTypeCompatible无论为均可
            //如果SelectionCriterionType为 exclusive，
            //pstMatchingRuleDescription->bExclusiveTypeCompatible只能为{"Includes", false}, {"Excludes", false}
            // {"Is", true}, {"IsNot", true}
            // Check compatibility if relevant
            if (!pSelectionCriterionType->isTypeInclusive() &&
                !pstMatchingRuleDescription->bExclusiveTypeCompatible) {
                strError = "Value incompatible with exclusive kind of type";
                return false;
            }

            // Store
            _eMatchesWhen = (MatchesWhen)matchesWhen;

            return true;
        }
    }

    strError = "Value not found";

    return false;
}







////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Rule check
bool CSelectionCriterionRule::matches() const
{
    //  const CSelectionCriterion *_pSelectionCriterion{nullptr}; // CSelectionCriterionRule::fromXml 中初始化
    assert(_pSelectionCriterion);

    // _eMatchesWhen 用来决定匹配的方式
    switch (_eMatchesWhen) {
    case EIs:
        return _pSelectionCriterion->is(_iMatchValue);  
    case EIsNot:
        return _pSelectionCriterion->isNot(_iMatchValue);
    case EIncludes:
        return _pSelectionCriterion->includes(_iMatchValue); //包含的意思
    case EExcludes:
        return _pSelectionCriterion->excludes(_iMatchValue);
    default:
        assert(0);
        return false;
    }
}