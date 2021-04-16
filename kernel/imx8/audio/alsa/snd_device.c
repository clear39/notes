
//  @   include/linux/kernel.h

//其中 ptr 为 member 的类型
//首先强制将 ptr 转成 type 类型，
//然后计算正确的骗的，得到type 类型指针地址
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:    the pointer to the member.      //成员指针实例
 * @type:   the type of the container struct this is embedded in.   // 包含成员的类型
 * @member: the name of the member within the struct.   //成员指针结构体
 *
 */
#define container_of(ptr, type, member) ({          \                                                                                                                                                          
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \      //这一行中 ptr 被转成 member 对应的类型
    (type *)( (char *)__mptr - offsetof(type,member) );})          // 进行偏移计算




//  @   include/linux/list.h

// 获取 
/**
 * list_entry - get the struct for this entry
 * @ptr:    the &struct list_head pointer.
 * @type:   the type of the struct this is embedded in.
 * @member: the name of the list_head within the struct.
 */
#define list_entry(ptr, type, member) container_of(ptr, type, member)

//  @   include/sound/core.h
#define snd_device(n) list_entry(n, struct snd_device, list)

struct snd_device {
    struct list_head list;      /* list of registered devices */
    struct snd_card *card;      /* card which holds this device */
    enum snd_device_state state;    /* state of the device */
    enum snd_device_type type;  /* device type */
    void *device_data;      /* device structure */
    struct snd_device_ops *ops; /* operations */
};
