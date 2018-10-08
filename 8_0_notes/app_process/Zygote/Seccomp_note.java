public final class Seccomp {
    public static final native void setPolicy();
}

//	frameworks/base/core/jni/android_os_seccomp.cpp
static void Seccomp_setPolicy(JNIEnv* /*env*/) {
	//			external/selinux/libselinux/src/getenforce.c   //	读取 /sys/fs/selinux/enforce
    if (security_getenforce() == 0) {
        ALOGI("seccomp disabled by setenforce 0");
        return;
    }

    if (!set_seccomp_filter()) {
        ALOGE("Failed to set seccomp policy - killing");
        exit(1);
    }
}


int security_getenforce(void)
{
	int fd, ret, enforce = 0;
	char path[PATH_MAX];
	char buf[20];

	if (!selinux_mnt) {
		errno = ENOENT;
		return -1;
	}

	snprintf(path, sizeof path, "%s/enforce", selinux_mnt);  // path == "/sys/fs/selinux/enforce"
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;

	memset(buf, 0, sizeof buf);
	ret = read(fd, buf, sizeof buf - 1);
	close(fd);
	if (ret < 0)
		return -1;

	if (sscanf(buf, "%d", &enforce) != 1)
		return -1;

	return !!enforce;
}




//	bionic/libc/kernel/uapi/linux/filter.h
struct sock_filter {
  __u16 code;
  __u8 jt;
  __u8 jf;
  __u32 k;
};

typedef std::vector<sock_filter> filter;

//		bionic/libc/seccomp/seccomp_policy.cpp
bool set_seccomp_filter() {
    filter f;

#ifdef DUAL_ARCH //DUAL_ARCH没有定义
    // Note that for mixed 64/32 bit architectures, ValidateArchitecture inserts a
    // jump that must be changed to point to the start of the 32-bit policy
    // 32 bit syscalls will not hit the policy between here and the call to SetJump
    auto offset_to_secondary_filter = ValidateArchitectureAndJumpIfNeeded(f);
#else
    ValidateArchitecture(f);//执行
#endif

    ExamineSyscall(f);

    for (size_t i = 0; i < primary_filter_size; ++i) {
        f.push_back(primary_filter[i]);
    }
    Disallow(f);

#ifdef DUAL_ARCH
    if (!SetValidateArchitectureJumpTarget(offset_to_secondary_filter, f)) {
        return false;
    }

    ExamineSyscall(f);

    for (size_t i = 0; i < secondary_filter_size; ++i) {
        f.push_back(secondary_filter[i]);
    }
    Disallow(f);
#endif

    return install_filter(f);
}


struct seccomp_data {
  int nr;
  __u32 arch;
  __u64 instruction_pointer;
  __u64 args[6];
};

static void ValidateArchitecture(filter& f) {
    f.push_back(BPF_STMT(BPF_LD|BPF_W|BPF_ABS, arch_nr));//#define arch_nr (offsetof(struct seccomp_data, arch))
    f.push_back(BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, PRIMARY_ARCH, 1, 0));
    Disallow(f);
}



inline void Disallow(filter& f) {
    f.push_back(BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRAP));
}

static void ExamineSyscall(filter& f) {
    f.push_back(BPF_STMT(BPF_LD|BPF_W|BPF_ABS, syscall_nr));//#define syscall_nr (offsetof(struct seccomp_data, nr))
}

//	bionic/libc/kernel/uapi/linux/filter.h
struct sock_fprog {
  unsigned short len;
  struct sock_filter __user * filter;
};


static bool install_filter(filter const& f) {
	
    struct sock_fprog prog = {
        static_cast<unsigned short>(f.size()),
        const_cast<struct sock_filter*>(&f[0]),
    };

    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) < 0) {
        PLOG(FATAL) << "Could not set seccomp filter of size " << f.size();
        return false;
    }

    LOG(INFO) << "Global filter of size " << f.size() << " installed";
    return true;
}





