

class CStringParameter : public CBaseParameter {

};



CStringParameter::CStringParameter(const string &strName, const CTypeElement *pTypeElement)
    : base(strName, pTypeElement)
{
	
}