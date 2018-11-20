//	@hardware/interfaces/configstore/1.0/default/service.cpp
int main() {
    configureRpcThreadpool(10, true);

    SetupMinijail("/vendor/etc/seccomp_policy/configstore@1.0.policy");

    sp<ISurfaceFlingerConfigs> surfaceFlingerConfigs = new SurfaceFlingerConfigs;
    status_t status = surfaceFlingerConfigs->registerAsService();
    LOG_ALWAYS_FATAL_IF(status != OK, "Could not register ISurfaceFlingerConfigs");

    // other interface registration comes here
    joinRpcThreadpool();
    return 0;
}

//		@hardware/interfaces/minijail/HardwareMinijail.cpp
void SetupMinijail(const std::string& seccomp_policy_path) {
    if (access(seccomp_policy_path.c_str(), R_OK) == -1) {
        LOG(WARNING) << "Could not find seccomp policy file at: " << seccomp_policy_path;
        return;
    }

    struct minijail* jail = minijail_new();
    if (jail == NULL) {
        LOG(FATAL) << "Failed to create minijail.";
    }

    minijail_no_new_privs(jail);
    minijail_log_seccomp_filter_failures(jail);
    minijail_use_seccomp_filter(jail);
    minijail_parse_seccomp_filters(jail, seccomp_policy_path.c_str());
    minijail_enter(jail);
    minijail_destroy(jail);
}


//	@external/minijail/libminijail.c
struct minijail API *minijail_new(void)
{
	return calloc(1, sizeof(struct minijail));
}

void API minijail_no_new_privs(struct minijail *j)
{
	j->flags.no_new_privs = 1;
}

void API minijail_log_seccomp_filter_failures(struct minijail *j)
{
	if (j->filter_len > 0 && j->filter_prog != NULL) {
		die("minijail_log_seccomp_filter_failures() must be called " "before minijail_parse_seccomp_filters()");
	}
	j->flags.seccomp_filter_logging = 1;
}

void API minijail_use_seccomp_filter(struct minijail *j)
{
	j->flags.seccomp_filter = 1;
}


void API minijail_parse_seccomp_filters(struct minijail *j, const char *path)
{
	if (!seccomp_should_parse_filters(j))
		return;

	FILE *file = fopen(path, "r");
	if (!file) {
		pdie("failed to open seccomp filter file '%s'", path);
	}

	if (parse_seccomp_filters(j, file) != 0) {
		die("failed to compile seccomp filter BPF program in '%s'", path);
	}
	fclose(file);
}




static int seccomp_should_parse_filters(struct minijail *j)
{
	if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, NULL) == -1) {
		/*
		 * |errno| will be set to EINVAL when seccomp has not been
		 * compiled into the kernel. On certain platforms and kernel
		 * versions this is not a fatal failure. In that case, and only
		 * in that case, disable seccomp and skip loading the filters.
		 */
		if ((errno == EINVAL) && seccomp_can_softfail()) {
			warn("not loading seccomp filters, seccomp filter not "    "supported");
			clear_seccomp_options(j);
			return 0;
		}
		/*
		 * If |errno| != EINVAL or seccomp_can_softfail() is false,
		 * we can proceed. Worst case scenario minijail_enter() will
		 * abort() if seccomp fails.
		 */
	}

	if (j->flags.seccomp_filter_tsync) {//j->flags.seccomp_filter_tsync == 0
		/* Are the seccomp(2) syscall and the TSYNC option supported? */
		if (sys_seccomp(SECCOMP_SET_MODE_FILTER,SECCOMP_FILTER_FLAG_TSYNC, NULL) == -1) {
			int saved_errno = errno;
			if (saved_errno == ENOSYS && seccomp_can_softfail()) {
				warn("seccomp(2) syscall not supported");
				clear_seccomp_options(j);
				return 0;
			} else if (saved_errno == EINVAL &&
				   seccomp_can_softfail()) {
				warn(
				    "seccomp filter thread sync not supported");
				clear_seccomp_options(j);
				return 0;
			}
			/*
			 * Similar logic here. If seccomp_can_softfail() is
			 * false, or |errno| != ENOSYS, or |errno| != EINVAL,
			 * we can proceed. Worst case scenario minijail_enter()
			 * will abort() if seccomp or TSYNC fail.
			 */
		}
	}
	return 1;
}


static int parse_seccomp_filters(struct minijail *j, FILE *policy_file)
{
	struct sock_fprog *fprog = malloc(sizeof(struct sock_fprog));
	int use_ret_trap =  j->flags.seccomp_filter_tsync || j->flags.seccomp_filter_logging;
	int allow_logging = j->flags.seccomp_filter_logging;

	if (compile_filter(policy_file, fprog, use_ret_trap, allow_logging)) {
		free(fprog);
		return -1;
	}

	j->filter_len = fprog->len;
	j->filter_prog = fprog;
	return 0;
}
