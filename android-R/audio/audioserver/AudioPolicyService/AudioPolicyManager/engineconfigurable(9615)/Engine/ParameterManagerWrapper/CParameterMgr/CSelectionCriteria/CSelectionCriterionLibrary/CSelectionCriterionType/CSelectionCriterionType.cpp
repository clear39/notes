// <criterion_type name="OutputDevicesMaskType" type="inclusive"/>
CSelectionCriterionType::CSelectionCriterionType(bool bIsInclusive) : _bInclusive(bIsInclusive)
{
    // For inclusive criterion type, appends the pair none,0 by default.
    if (_bInclusive) {
    	 //std::map<std::string, int> _numToLitMap;
        _numToLitMap["none"] = 0;
    }
}



bool CSelectionCriterionType::isTypeInclusive() const
{
    return _bInclusive;
}


bool CSelectionCriterionType::getNumericalValue(const std::string &strValue, int &iValue) const
{
    if (_bInclusive) {
        Tokenizer tok(strValue, _strDelimiter); // _strDelimiter = "|";
        std::vector<std::string> astrValues = tok.split();
        size_t uiNbValues = astrValues.size();
        int iResult = 0;
        size_t uiValueIndex;
        iValue = 0;

        // Looping on each std::string delimited by "|" token and adding the associated value
        for (uiValueIndex = 0; uiValueIndex < uiNbValues; uiValueIndex++) {

            if (!getAtomicNumericalValue(astrValues[uiValueIndex], iResult)) {

                return false;
            }
            iValue |= iResult;
        }
        return true;
    }
    return getAtomicNumericalValue(strValue, iValue);
}



bool CSelectionCriterionType::getAtomicNumericalValue(const std::string &strValue, int &iValue) const
{
    auto it = _numToLitMap.find(strValue);

    if (it != _numToLitMap.end()) {

        iValue = it->second;

        return true;
    }
    return false;
}
