struct snd_device_ops {
    int (*dev_free)(struct snd_device *dev);
    int (*dev_register)(struct snd_device *dev);
    int (*dev_disconnect)(struct snd_device *dev);
};
