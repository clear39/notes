



bool CPluginLocation::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext & /*ctx*/)
{
    // Retrieve folder
    xmlElement.getAttribute("Folder", _strFolder);

    // Get Info from children
    CXmlElement::CChildIterator childIterator(xmlElement);

    CXmlElement xmlPluginElement;

    while (childIterator.next(xmlPluginElement)) {

        // Fill Plugin List
        // std::list<std::string> _pluginList;
        _pluginList.push_back(xmlPluginElement.getNameAttribute());
    }

    // Don't dig
    return true;
}