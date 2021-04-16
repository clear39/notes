
class CBaseParameter : public CInstanceConfigurableElement {

};

class CParameter : public CBaseParameter {

};

// pTypeElement = CParameterType
CParameter::CParameter(const string &strName, const CTypeElement *pTypeElement)
    : base(strName, pTypeElement)
{

}