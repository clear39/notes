

class CMappingData
{
    typedef std::map<std::string, std::string>::const_iterator KeyToValueMapConstIterator;

public:
    /** Initialize mapping data through a raw value
     *
     * @param[in] rawMapping the raw mapping data which has to be parsed.
     *            This raw value is a succession of pair "key:value" separated with comma.
     * @param[out] error description of the error if there is one, empty otherwise
     * @return true on success, false otherwise
     */
    bool init(const std::string &rawMapping, std::string &error);

    // Query
    bool getValue(const std::string &strkey, const std::string *&pStrValue) const;

    /**
     * Formats the mapping as a list of comma-space separated key:value pairs
     *
     * @return the formatted std::string
     */
    std::string asString() const;

private:
    bool addValue(const std::string &strkey, const std::string &strValue);

    std::map<std::string, std::string> _keyToValueMap;
};


bool CMappingData::init(const std::string &rawMapping, std::string &error)
{
    Tokenizer mappingTok(rawMapping, ",");

    std::string strMappingElement;

    for (const auto &strMappingElement : mappingTok.split()) {

        std::string::size_type iFistDelimiterOccurrence = strMappingElement.find_first_of(':');

        std::string strKey, strValue;

        if (iFistDelimiterOccurrence == std::string::npos) {

            // There is no delimiter in the mapping field,
            // it means that no value has been provided
            strKey = strMappingElement;
            strValue = "";

        } else {

            // Get mapping key
            strKey = strMappingElement.substr(0, iFistDelimiterOccurrence);

            // Get mapping value
            strValue = strMappingElement.substr(iFistDelimiterOccurrence + 1);
        }

        if (!addValue(strKey, strValue)) {
            error = "Unable to process Mapping element key = " + strKey + ", value = " + strValue +
                    ": Duplicate Mapping data";

            return false;
        }
    }
    return true;
}


bool CMappingData::addValue(const std::string &strkey, const std::string &strValue)
{
    if (_keyToValueMap.find(strkey) != _keyToValueMap.end()) {

        return false;
    }
    _keyToValueMap[strkey] = strValue;

    return true;
}
