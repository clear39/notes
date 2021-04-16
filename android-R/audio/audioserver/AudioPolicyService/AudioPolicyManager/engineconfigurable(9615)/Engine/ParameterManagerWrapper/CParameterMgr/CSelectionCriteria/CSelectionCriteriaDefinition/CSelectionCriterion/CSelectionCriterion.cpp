
CSelectionCriterion::CSelectionCriterion(const std::string &strName,
                                         const CSelectionCriterionType *pType,
                                         core::log::Logger &logger)
    : base(strName), _pType(pType), _logger(logger)
{
}




/// From ISelectionCriterionInterface
// State
void CSelectionCriterion::setCriterionState(int iState)
{
    // 对于 AvailableOutputDevices 类型的CSelectionCriterion 成员_iState的值就是对应 audio_devices_t 的值 
    // Check for a change
    if (_iState != iState) {

        _iState = iState;

        _logger.info() << "Selection criterion changed event: " << getFormattedDescription(false, false);

        // Check if the previous criterion value has been taken into account (i.e. at least one
        // Configuration was applied
        // since the last criterion change)
        if (_uiNbModifications != 0) {

            _logger.warning() << "Selection criterion '" << getName() << "' has been modified "
                              << _uiNbModifications
                              << " time(s) without any configuration application";
        }

        // Track the number of modifications for this criterion
        _uiNbModifications++;
    }
}

int CSelectionCriterion::getCriterionState() const
{
    return _iState;
}

// Type
const ISelectionCriterionTypeInterface *CSelectionCriterion::getCriterionType() const
{
	//const CSelectionCriterionType *_pType;
    return _pType; 
}


/// Match methods
bool CSelectionCriterion::is(int iState) const
{
    return _iState == iState;
}

bool CSelectionCriterion::isNot(int iState) const
{
    return _iState != iState;
}

bool CSelectionCriterion::includes(int iState) const
{
    // For inclusive criterion, Includes checks if ALL the bit sets in iState are set in the
    // current _iState.
    return (_iState & iState) == iState;
}

bool CSelectionCriterion::excludes(int iState) const
{
    return (_iState & iState) == 0;
}