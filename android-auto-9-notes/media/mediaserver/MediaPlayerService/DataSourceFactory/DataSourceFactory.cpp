
//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libstagefright/DataSourceFactory.cpp

sp<DataSource> DataSourceFactory::CreateMediaHTTP(const sp<MediaHTTPService> &httpService) {
    if (httpService == NULL) {
        return NULL;
    }

    sp<MediaHTTPConnection> conn = httpService->makeHTTPConnection();
    if (conn == NULL) {
        return NULL;
    } else {
        return new MediaHTTP(conn);
    }
}