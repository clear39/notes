

class CCompoundRule : public CRule {

}


/*
<CompoundRule Type="All">
  	<SelectionCriterionRule SelectionCriterion="AvailableOutputDevices" MatchesWhen="Includes" Value="Speaker"/>
</CompoundRule>

*/
// From IXmlSink
bool CCompoundRule::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Get type
    string strType;
    xmlElement.getAttribute("Type", strType);
    //const char *CCompoundRule::_apcTypes[2] = {"Any", "All"};
    // bool _bTypeAll{false};
    // 只有 strType 为 "All"的时候,_bTypeAll 为 true
    _bTypeAll = strType == _apcTypes[true];

    // Base
    // CElement::fromXml
    // pParameterConfigurationLibrary->addElementBuilder("SelectionCriterionRule", new TElementBuilderTemplate<CSelectionCriterionRule>());
    // CSelectionCriterionRule

    // pParameterConfigurationLibrary->addElementBuilder("CompoundRule",new TElementBuilderTemplate<CCompoundRule>());
    // CCompoundRule
    return base::fromXml(xmlElement, serializingContext);
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Rule check
bool CCompoundRule::matches() const
{
    size_t uiChild;
    // CSelectionCriterionRule
    size_t uiNbChildren = getNbChildren();

    for (uiChild = 0; uiChild < uiNbChildren; uiChild++) {

        // pRule = CSelectionCriterionRule
        const CRule *pRule = static_cast<const CRule *>(getChild(uiChild));

        // _bTypeAll 只有 CompoundRule标签中Type属性值为"All"，才为true 
        // 注意只有 pRule->matches() 为 false的时候 pRule->matches() ^ _bTypeAll 才为 true
        if (pRule->matches() ^ _bTypeAll) {
            return !_bTypeAll;
        }
    }
    return _bTypeAll;
}