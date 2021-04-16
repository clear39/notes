

template <class CDefaultElementBuilder>
class CDefaultElementLibrary : public CElementLibrary
{
public:
    ~CDefaultElementLibrary() override = default;

    /** Set the default builder used in fallback mechanism.
      * @see createElement() for more detail on this mechanism.
      *
      * @param[in] defaultBuilder if NULL default builder mechanism, else provided builder is used.
      */
    void setDefaultBuilder(std::unique_ptr<CDefaultElementBuilder> defaultBuilder)
    {
        _defaultBuilder = std::move(defaultBuilder);
    }

    /** Create and return an element instanciated depending on an xmlElement.
      *
      * @param[in] xmlElement: The xml element used to find a matching builder
      *
      * @return If a matching builder is found, return an element created from the builder,
      *         otherwise:
      *             If the default mechanism is enable (@see enableDefaultMechanism),
      *                 create the elemen with the default element builder.
      *             otherwise, return NULL.
      */
    CElement *createElement(const CXmlElement &xmlElement) const;

private:
    std::unique_ptr<CDefaultElementBuilder> _defaultBuilder;
};

template <class CDefaultElementBuilder>
CElement *CDefaultElementLibrary<CDefaultElementBuilder>::createElement(
    const CXmlElement &xmlElement) const
{
    CElement *builtElement = CElementLibrary::createElement(xmlElement);

    if (builtElement != nullptr) {
        // The element was created, return it
        return builtElement;
    }

    if (_defaultBuilder == nullptr) {
        // The default builder mechanism is not enabled
        return nullptr;
    }

    // Use the default builder
    return _defaultBuilder->createElement(xmlElement);
}