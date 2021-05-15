
#define CHRDEV_MAJOR_HASH_SIZE 255


static struct char_device_struct {                                                                                                                                                                             
    struct char_device_struct *next;
    unsigned int major;         //主设备id 
    unsigned int baseminor;     //从设备起始id
    int minorct;                //从设备个数
    char name[64];              //设备名称或者驱动名称
    struct cdev *cdev;      /* will die */
} *chrdevs[CHRDEV_MAJOR_HASH_SIZE];
