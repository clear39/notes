

/**
 * 
*/
sp<DataSource> DataSourceFactory::CreateFromFd(int fd, int64_t offset, int64_t length) {
    sp<FileSource> source = new FileSource(fd, offset, length);
    return source->initCheck() != OK ? nullptr : source;
}