/// PFW related definitions
// Logger
class ParameterMgrPlatformConnectorLogger : public CParameterMgrPlatformConnector::ILogger
{
public:
    ParameterMgrPlatformConnectorLogger() {}

    virtual void info(const string &log)
    {
        ALOGV("policy-parameter-manager: %s", log.c_str());
    }
    virtual void warning(const string &log)
    {
        ALOGW("policy-parameter-manager: %s", log.c_str());
    }
};