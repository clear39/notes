sp<IServiceManager> sm = defaultServiceManager();
//转换得到
sp<IServiceManager> sm = new BpServiceManager(BpBinder(0));

virtual sp<IBinder> BpServiceManager::getService(const String16& name) const
{
	unsigned n;
	for (n = 0; n < 5; n++){
		sp<IBinder> svc = checkService(name);
		if (svc != NULL) return svc;
		LOGI("Waiting for service %s...\n", String8(name).string());
		sleep(1);
	}
	return NULL;
}

virtual sp<IBinder> BpServiceManager::checkService( const String16& name) const
{
	Parcel data, reply;
	data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());
	data.writeString16(name);
	remote()->transact(CHECK_SERVICE_TRANSACTION, data, &reply);
	return reply.readStrongBinder();
}


