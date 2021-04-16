
//  @   external/parameter-framework/upstream/parameter/ParameterMgrPlatformConnector.cpp
// Construction
CParameterMgrPlatformConnector::CParameterMgrPlatformConnector(
    const string &strConfigurationFilePath)
    : _pParameterMgrLogger(new CParameterMgrLogger<CParameterMgrPlatformConnector>(*this)),
      _pParameterMgr(new CParameterMgr(strConfigurationFilePath, *_pParameterMgrLogger)),
      _bStarted(false), _pLogger(nullptr)
{
}

// 在 ParameterManagerWrapper 的构造函数中使用 ，bValidate 为false
bool CParameterMgrPlatformConnector::setValidateSchemasOnStart(bool bValidate,
                                                               std::string &strError)
{
    if (_bStarted) {

        strError = "Can not enable xml validation after the start of the parameter-framework";
        return false;
    }

    _pParameterMgr->setValidateSchemasOnStart(bValidate);
    return true;
}

// ParameterManagerWrapper::addCriterion 中调用
// Selection Criteria interface. Beware returned objects are lent, clients shall not delete them!
ISelectionCriterionTypeInterface *CParameterMgrPlatformConnector::createSelectionCriterionType(
    bool bIsInclusive)
{
    assert(!_bStarted);

    return _pParameterMgr->createSelectionCriterionType(bIsInclusive);
}

ISelectionCriterionInterface *CParameterMgrPlatformConnector::createSelectionCriterion(
    const string &strName, const ISelectionCriterionTypeInterface *pSelectionCriterionType)
{
    assert(!_bStarted);

    return _pParameterMgr->createSelectionCriterion(
        strName, static_cast<const CSelectionCriterionType *>(pSelectionCriterionType));
}



// Start
bool CParameterMgrPlatformConnector::start(string &strError)
{
    // Create data structure
    if (!_pParameterMgr->load(strError)) {

        return false;
    }

    _bStarted = true;

    return true;
}





//////////////////////////////////////////////////////////////////////////////
// Configuration application
void CParameterMgrPlatformConnector::applyConfigurations()
{
    assert(_bStarted);

    _pParameterMgr->applyConfigurations();
}
