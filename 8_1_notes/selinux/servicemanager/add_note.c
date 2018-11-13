//	@frameworks/native/cmds/servicemanager/service_manager.c
static int svc_can_register(const uint16_t *name, size_t name_len, pid_t spid, uid_t uid)
{
    const char *perm = "add";

    if (multiuser_get_app_id(uid) >= AID_APP) {//	@system/core/libcutils/multiuser.c:24
        return 0; /* Don't allow apps to register services */
    }

    return check_mac_perms_from_lookup(spid, uid, perm, str8(name, name_len)) ? 1 : 0;
}



static bool check_mac_perms_from_lookup(pid_t spid, uid_t uid, const char *perm, const char *name)
{
    bool allowed;
    char *tctx = NULL;

    if (!sehandle) { //sehandle为servicemanager启动时初始化
        ALOGE("SELinux: Failed to find sehandle. Aborting service_manager.\n");
        abort();
    }

    //	@external/selinux/libselinux/src/label.c:390
    if (selabel_lookup(sehandle, &tctx, name, 0) != 0) {
        ALOGE("SELinux: No match for %s in service_contexts.\n", name);
        return false;
    }

    allowed = check_mac_perms(spid, uid, tctx, perm, name);
    freecon(tctx);
    return allowed;
}


int selabel_lookup(struct selabel_handle *rec, char **con,const char *key, int type)
{
	struct selabel_lookup_rec *lr;

	lr = selabel_lookup_common(rec, 1, key, type);
	if (!lr)
		return -1;

	*con = strdup(lr->ctx_trans);
	return *con ? 0 : -1;
}


static struct selabel_lookup_rec *selabel_lookup_common(struct selabel_handle *rec, int translating,const char *key, int type)
{
	struct selabel_lookup_rec *lr;
	char *ptr = NULL;

	if (key == NULL) {
		errno = EINVAL;
		return NULL;
	}

	ptr = selabel_sub_key(rec, key);
	if (ptr) {
		lr = rec->func_lookup(rec, ptr, type);
		free(ptr);
	} else {
		lr = rec->func_lookup(rec, key, type);
	}
	if (!lr)
		return NULL;

	if (selabel_fini(rec, lr, translating))
		return NULL;

	return lr;
}



static bool check_mac_perms(pid_t spid, uid_t uid, const char *tctx, const char *perm, const char *name)
{
    char *sctx = NULL;
    const char *class = "service_manager";
    bool allowed;
    struct audit_data ad;

    if (getpidcon(spid, &sctx) < 0) {
        ALOGE("SELinux: getpidcon(pid=%d) failed to retrieve pid context.\n", spid);
        return false;
    }

    ad.pid = spid;
    ad.uid = uid;
    ad.name = name;

    int result = selinux_check_access(sctx, tctx, class, perm, (void *) &ad);
    allowed = (result == 0);

    freecon(sctx);
    return allowed;
}
