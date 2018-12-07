//	@system/core/libsysutils/src/NetlinkEvent.cpp






void NetlinkEvent::dump() {
    int i;

    for (i = 0; i < NL_PARAMS_MAX; i++) {
        if (!mParams[i])
            break;
        SLOGD("NL param '%s'\n", mParams[i]);
    }
}



const char *NetlinkEvent::findParam(const char *paramName) {
    size_t len = strlen(paramName);
    for (int i = 0; i < NL_PARAMS_MAX && mParams[i] != NULL; ++i) {
        const char *ptr = mParams[i] + len;
        if (!strncmp(mParams[i], paramName, len) && *ptr == '=')
            return ++ptr;
    }

    SLOGE("NetlinkEvent::FindParam(): Parameter '%s' not found", paramName);
    return NULL;
}



