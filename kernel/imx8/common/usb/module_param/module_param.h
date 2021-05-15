
https://blog.csdn.net/zhudaozhuan/article/details/52214438

https://blog.csdn.net/sttypxx520/article/details/2579679?utm_medium=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param&depth_1-utm_source=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.channel_param


static bool usbfs_snoop;
module_param(usbfs_snoop, bool, S_IRUGO | S_IWUSR);                                                                                                                         
MODULE_PARM_DESC(usbfs_snoop, "true to log all usbfs traffic");


//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
module_param(usbfs_snoop, bool, S_IRUGO | S_IWUSR); 

#define module_param(name, type, perm)              \         // module_param(usbfs_snoop, bool, S_IRUGO | S_IWUSR);                                                                                                                
    module_param_named(name, name, type, perm)                // module_param_named(usbfs_snoop, usbfs_snoop, bool, S_IRUGO | S_IWUSR)



//module_param_named(usbfs_snoop, usbfs_snoop, bool, S_IRUGO | S_IWUSR)

#define module_param_named(name, value, type, perm)            \
    param_check_##type(name, &(value));                \      // param_check_bool(usbfs_snoop,&usbfs_snoop)
    module_param_cb(name, &param_ops_##type, &value, perm);   \   // module_param_cb(usbfs_snoop,&param_ops_bool,&usbfs_snoop,S_IRUGO | S_IWUSR)
    __MODULE_PARM_TYPE(name, #type)                                 // __MODULE_PARM_TYPE(usbfs_snoop,bool)


//param_check_bool(usbfs_snoop,&usbfs_snoop)
#define param_check_bool(name, p) __param_check(name, p, bool)    // __param_check(usbfs_snoop, &usbfs_snoop, bool)


/* All the helper functions */
/* The macros to do compile-time type checking stolen from Jakub
   Jelinek, who IIRC came up with this idea for the 2.4 module init code. */
#define __param_check(name, p, type) \                                                                                                                                      
    static inline type __always_unused *__check_##name(void) { return(p); }   // static inline bool __always_unused *__check_usbfs_snoop(void) { return(&usbfs_snoop); } 


///////////////////////////////////////////////////////////////////////////////////////////

// module_param_cb(usbfs_snoop,&param_ops_bool,&usbfs_snoop,S_IRUGO | S_IWUSR)
#define module_param_cb(name, ops, arg, perm)                     \
    __module_param_call(MODULE_PARAM_PREFIX, name, ops, arg, perm, -1, 0)  //__module_param_call(MODULE_PARAM_PREFIX, usbfs_snoop, &param_ops_bool, &usbfs_snoop, S_IRUGO | S_IWUSR, -1, 0)



// include/linux/moduleparam.h


/* You can override this manually, but generally this should match the
   module name. */
#ifdef MODULE                                                                                                                                                               
#define MODULE_PARAM_PREFIX /* empty */
#else
#define MODULE_PARAM_PREFIX KBUILD_MODNAME "."
#endif


struct kernel_param_ops {
    /* How the ops should behave */
    unsigned int flags;
    /* Returns 0, or -errno.  arg is in kp->arg. */
    int (*set)(const char *val, const struct kernel_param *kp);
    /* Returns length written or -errno.  Buffer is 4k (ie. be short!) */
    int (*get)(char *buffer, const struct kernel_param *kp);
    /* Optional function to free kp->arg when module unloaded. */
    void (*free)(void *arg);
};



struct kernel_param {                                                                                                                                                       
    const char *name;
    struct module *mod;
    const struct kernel_param_ops *ops;
    const u16 perm;
    s8 level;
    u8 flags;
    union {
        void *arg;
        const struct kparam_string *str;
        const struct kparam_array *arr;
    };
};

const struct kernel_param_ops param_ops_bool = {                                                                                                                            
    .flags = KERNEL_PARAM_OPS_FL_NOARG,
    .set = param_set_bool,
    .get = param_get_bool,
};
EXPORT_SYMBOL(param_ops_bool);

/* Actually could be a bool or an int, for historical reasons. */
int param_set_bool(const char *val, const struct kernel_param *kp)                                                                                                          
{
    /* No equals means "set"... */
    if (!val) val = "1";

    /* One of =[yYnN01] */
    return strtobool(val, kp->arg);
}
EXPORT_SYMBOL(param_set_bool);


int param_get_bool(char *buffer, const struct kernel_param *kp)
{
    /* Y and N chosen as being relatively non-coder friendly */
    return sprintf(buffer, "%c\n", *(bool *)kp->arg ? 'Y' : 'N');
}
EXPORT_SYMBOL(param_get_bool);




/* This is the fundamental function for registering boot/module
   parameters. */
#define __module_param_call(prefix, name, ops, arg, perm, level, flags) \                                                                                                   
    /* Default value instead of permissions? */         \
    static const char __param_str_##name[] = prefix #name;      \               // static const char __param_str_usbfs_snoop[] = MODULE_PARAM_PREFIX usbfs_snoop;
    static struct kernel_param __moduleparam_const __param_##name   \           // static struct kernel_param __moduleparam_const __param_usbfs_snoop
    __used                              \                                       // __used
    __attribute__ ((unused,__section__ ("__param"),aligned(sizeof(void *)))) \  // __attribute__ ((unused,__section__ ("__param"),aligned(sizeof(void *))))
    = { __param_str_##name, THIS_MODULE, ops,           \                       // = { __param_str_usbfs_snoop, THIS_MODULE, &param_ops_bool,
        VERIFY_OCTAL_PERMISSIONS(perm), level, flags, { arg } }                 // VERIFY_OCTAL_PERMISSIONS(S_IRUGO | S_IWUSR), -1, 0, {  &usbfs_snoop } }




///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

// MODULE_PARM_DESC(usbfs_snoop, "true to log all usbfs traffic");
#define MODULE_PARM_DESC(_parm, desc) \                                                                                                                                     
    __MODULE_INFO(parm, _parm, #_parm ":" desc)    // __MODULE_INFO(parm, usbfs_snoop, usbfs_snoop ":" "true to log all usbfs traffic" ) 


#define MODULE 1
#ifdef MODULE
#define __MODULE_INFO(tag, name, info)                    \                                                                                                                 
static const char __UNIQUE_ID(name)[]                     \    // static const char __UNIQUE_ID(usbfs_snoop)[] 
  __used __attribute__((section(".modinfo"), unused, aligned(1)))     \   
  = __stringify(tag) "=" info                                           //  = __stringify(parm) "=" usbfs_snoop ":" "true to log all usbfs traffic"  
#else  /* !MODULE */
/* This struct is here for syntactic coherency, it is not used */
#define __MODULE_INFO(tag, name, info)                    \
  struct __UNIQUE_ID(name) {}
#endif

