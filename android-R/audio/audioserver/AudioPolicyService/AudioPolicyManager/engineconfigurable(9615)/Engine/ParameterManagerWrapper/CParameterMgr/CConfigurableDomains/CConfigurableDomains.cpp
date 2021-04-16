
class CConfigurableDomains : public CElement {

};


// From IXmlSink
bool CElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext) try {

    // ConfigurableDomains没有该属性
    xmlElement.getAttribute(gDescriptionPropertyName, _strDescription);

    // Propagate through children
    CXmlElement::CChildIterator childIterator(xmlElement);

    CXmlElement childElement;

    while (childIterator.next(childElement)) {

        CElement *pChild;

        // childrenAreDynamic返回true
        if (!childrenAreDynamic()) {

            pChild = findChildOfKind(childElement.getType());

            if (!pChild) {

                serializingContext.setError("Unable to handle XML element: " + childElement.getPath());

                return false;
            }

        } else {
            // Child needs creation
            // pParameterConfigurationLibrary->addElementBuilder("ConfigurableDomain", new TElementBuilderTemplate<CConfigurableDomain>());
            // pChild = CConfigurableDomain
            pChild = createChild(childElement, serializingContext);
            if (!pChild) {
                return false;
            }
        }

        // Dig
        // pChild = CConfigurableDomain
        if (!pChild->fromXml(childElement, serializingContext)) {

            return false;
        }
    }

    return true;
} catch (const PfError &e) {
    serializingContext.appendLineToError(e.what());
    return false;
}




/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
// Configuration application if required
void CConfigurableDomains::apply(CParameterBlackboard *pParameterBlackboard, CSyncerSet &syncerSet,
                                 bool bForce, core::Results &infos) const
{
    /// Delegate to domains

    // Start with domains that can be synchronized all at once (with passed syncer set)
    size_t uiNbConfigurableDomains = getNbChildren();
    
    for (size_t child = 0; child < uiNbConfigurableDomains; child++) {

        const CConfigurableDomain *pChildConfigurableDomain =
            static_cast<const CConfigurableDomain *>(getChild(child));

        std::string info;
        // Apply and collect syncers when relevant
        // pChildConfigurableDomain = CConfigurableDomain         
        pChildConfigurableDomain->apply(pParameterBlackboard, &syncerSet, bForce, info);

        if (!info.empty()) {
            infos.push_back(info);
        }
    }
    
    // Synchronize those collected syncers
    syncerSet.sync(*pParameterBlackboard, false, nullptr);

    // Then deal with domains that need to synchronize along apply
    for (size_t child = 0; child < uiNbConfigurableDomains; child++) {

        const CConfigurableDomain *pChildConfigurableDomain =
            static_cast<const CConfigurableDomain *>(getChild(child));

        std::string info;
        // Apply and synchronize when relevant
        pChildConfigurableDomain->apply(pParameterBlackboard, nullptr, bForce, info);
        if (!info.empty()) {
            infos.push_back(info);
        }
    }
}