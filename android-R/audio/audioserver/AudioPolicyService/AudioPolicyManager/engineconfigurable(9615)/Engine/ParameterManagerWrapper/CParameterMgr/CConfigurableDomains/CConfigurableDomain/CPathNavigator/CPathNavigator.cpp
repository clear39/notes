
class CPathNavigator
{

};
// /Policy/policy/product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker
CPathNavigator::CPathNavigator(const std::string &strPath)
{
    init(strPath);
}

void CPathNavigator::init(const std::string &strPath)
{
    Tokenizer tokenizer(strPath, "/");

    _astrItems = tokenizer.split();

    // _bValid 为 true
    _bValid = checkPathFormat(strPath);
}


bool CPathNavigator::checkPathFormat(const std::string &strUpl)
{
    return strUpl[0] == '/';
}



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Navigate through
bool CPathNavigator::navigateThrough(const std::string &strItemName, std::string &strError)
{
    if (!_bValid) {

        strError = "Path not well formed: " + getCurrentPath();

        return false;
    }

    std::string *pStrChildName = next();

    if (!pStrChildName) {

        strError =
            "Path not complete: " + getCurrentPath() + ", trying to access to " + strItemName;

        return false;
    }

    if (*pStrChildName != strItemName) {

        strError = "Path not found: " + getCurrentPath() + ", expected: " + strItemName +
                   " but found: " + *pStrChildName;

        return false;
    }

    return true;
}

std::string *CPathNavigator::next()
{
    if (_currentIndex < _astrItems.size()) {

        return &_astrItems[_currentIndex++];
    }

    return nullptr;
}