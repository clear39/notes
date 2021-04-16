/*
<Component Name="streams" Type="Streams"/>  （CComponentInstance）
<Component Name="input_sources" Type="InputSources"/> （CComponentInstance）
<Component Name="product_strategies" Type="ProductStrategies"/> （CComponentInstance）
*/
// strName 得到上面对应的 Name 属性值
// pTypeElement 为上面对应的三个 CComponentInstance 

class CComponent : public CInstanceConfigurableElement 
{
public:

    CComponent(const std::string &strName, const CTypeElement *pTypeElement)
        : CInstanceConfigurableElement(strName, pTypeElement)
    {
    }

    // Type
    Type getType() const override { return EComponent; }
};


/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
// IMapper 只有 CSubsystem 实现，mapper为PolicySubsystem
// 最开始执行是CSubsystem::mapSubsystemElements()中调用，PolicySubsystem的三个子成员均没有CMappingData数据，所以直接遍历子对象；

bool CInstanceConfigurableElement::map(IMapper &mapper, std::string &strError)
{
    //  
    bool bHasMappingData = getTypeElement()->hasMappingData();
    bool bKeepDiving = true;

    // Begin
    //只有有CMappingData才会执行 CSubsystem.mapBegin函数
    // 如果mapBegin创建 Stream InputSource ProductStrategy 三种类，bKeepDiving为false
    if (bHasMappingData && !mapper.mapBegin创建了(this, bKeepDiving, strError)) {
        return false;
    }

    // Go on through children?
    if (bKeepDiving) {

        // Map children
        size_t uiNbChildren = getNbChildren();
        size_t uiChild;

        for (uiChild = 0; uiChild < uiNbChildren; uiChild++) {

            // pInstanceConfigurableChildElement = CComponent
            CInstanceConfigurableElement *pInstanceConfigurableChildElement =
                static_cast<CInstanceConfigurableElement *>(getChild(uiChild));

            if (!pInstanceConfigurableChildElement->map(mapper, strError)) {

                return false;
            }
        }
    }

    // End
    if (bHasMappingData) {

        mapper.mapEnd();
    }
    return true;
}


// Type element
const CTypeElement *CInstanceConfigurableElement::getTypeElement() const
{
    //
    return _pTypeElement;
}


// Mapping
bool CInstanceConfigurableElement::getMappingData(const std::string &strKey,
                                                  const std::string *&pStrValue) const
{
    // Delegate
    return getTypeElement()->getMappingData(strKey, pStrValue);
}

// Memory
size_t CConfigurableElement::getFootPrint() const
{
    size_t uiSize = 0;
    size_t uiNbChildren = getNbChildren();

    for (size_t index = 0; index < uiNbChildren; index++) {

        const CConfigurableElement *pConfigurableElement =
            static_cast<const CConfigurableElement *>(getChild(index));

        uiSize += pConfigurableElement->getFootPrint();
    }

    return uiSize;
}


/*
在 PolicySubsystem::findDescendant 中调用
*/
CElement *CElement::findDescendant(CPathNavigator &pathNavigator)
{
	// transmitted_through_speaker/selected_output_devices/mask/speaker
    string *pStrChildName = pathNavigator.next();  // transmitted_through_speaker

    if (!pStrChildName) {

        return this;
    }

    //这里同样找到 CComponent  
    CElement *pChild = findChild(*pStrChildName);

    if (!pChild) {

        return nullptr;
    }

    return pChild->findDescendant(pathNavigator);
}
