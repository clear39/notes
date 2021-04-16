


/*
pParameterCreationLibrary->addElementBuilder(
        "Subsystem", new CSubsystemElementBuilder(getSystemClass()->getSubsystemLibrary()));

// getSubsystemLibrary 得到 _pSubsystemLibrary(通过CSystemClass::loadSubsystems) 中添加的库集合
 _pSubsystemLibrary->addElementBuilder("Virtual", new VirtualSubsystemBuilder(_logger));
 _pSubsystemLibrary->addElementBuilder("Policy", new TLoggingElementBuilderTemplate<PolicySubsystem>(logger));
*/

//_pSubsystemLibrary（CSubsystemLibrary） 中有俩个 Builder (VirtualSubsystemBuilder和TLoggingElementBuilderTemplate)
CSubsystemElementBuilder::CSubsystemElementBuilder(const CSubsystemLibrary *pSubsystemLibrary)
    : CElementBuilder(), _pSubsystemLibrary(pSubsystemLibrary)
{
}


CElement *CSubsystemElementBuilder::createElement(const CXmlElement &xmlElement) const
{
	//由于 CSubsystemLibrary 没有实现 createElement 函数，
	//调用父类CElement *CDefaultElementLibrary<CDefaultElementBuilder>::createElement
	//在父类中调用 CElementLibrary::createElement （CDefaultElementLibrary继承CElementLibrary）
	// CElementLibrary::createElement 会调用 getBuilderType 函数获取 "Type"的属性值，作为查找的key，这里为"Policy"
	// 最终调用 TLoggingElementBuilderTemplate->createElement(xmlElement)
    return _pSubsystemLibrary->createElement(xmlElement);
}
