#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/initval.h>

/* module parameters (see "Module Parameters") */
/* SNDRV_CARDS: maximum number of cards supported by this module */
static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;

/* definition of the chip-specific record */
struct mychip {
        struct snd_card *card;
        /* the rest of the implementation will be in section "PCI Resource Management" */
};

/* chip-specific destructor
* (see "PCI Resource Management")
*/
static int snd_mychip_free(struct mychip *chip)
{
        .... /* will be implemented later... */
}

/* component-destructor
* (see "Management of Cards and Components")
*/
static int snd_mychip_dev_free(struct snd_device *device)
{
        return snd_mychip_free(device->device_data);
}



static int snd_mychip_create(struct snd_card *card,struct pci_dev *pci,struct mychip **rchip)
{
    struct mychip *chip;
    int err;
    static struct snd_device_ops ops = {
        .dev_free = snd_mychip_dev_free,
    };
    
    *rchip = NULL;
    
    /**
     *  check PCI availability here  (see "PCI Resource Management")
    */
    ....
    
    /* allocate a chip-specific data with zero filled */
    chip = kzalloc(sizeof(*chip), GFP_KERNEL);
    if (chip == NULL)
            return -ENOMEM;
    
    chip->card = card;
    
    /**
     *  rest of initialization here; will be implemented later, see "PCI Resource Management"
    */
    ....
    
    err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, chip, &ops);
    if (err < 0) {
        snd_mychip_free(chip);
        return err;
    }

    *rchip = chip;
    return 0;
}


/* constructor -- see "Constructor" sub-section */
static int snd_mychip_probe(struct pci_dev *pci, const struct pci_device_id *pci_id)
{
    static int dev;
    struct snd_card *card;
    struct mychip *chip;
    int err;

    /* (1) */
    if (dev >= SNDRV_CARDS)
        return -ENODEV;
    if (!enable[dev]) {
        dev++; 
        return -ENOENT;
    }
    
    /* (2) */
    err = snd_card_new(&pci->dev, index[dev], id[dev], THIS_MODULE,0, &card);
    if (err < 0)
        return err;

    /* (3) */
    err = snd_mychip_create(card, pci, &chip);
    if (err < 0) {
        snd_card_free(card);
        return err;
    }

    /* (4) */
    strcpy(card->driver, "My Chip");
    strcpy(card->shortname, "My Own Chip 123");
    sprintf(card->longname, "%s at 0x%lx irq %i",card->shortname, chip->ioport, chip->irq);
    
    /* (5) */
    .... /* implemented later */
    
    /* (6) */
    err = snd_card_register(card);
    if (err < 0) {
            snd_card_free(card);
            return err;
    }

    /* (7) */
    pci_set_drvdata(pci, card);
    dev++;
    return 0;
}


/* destructor -- see the "Destructor" sub-section */
static void snd_mychip_remove(struct pci_dev *pci)
{
        snd_card_free(pci_get_drvdata(pci));
        pci_set_drvdata(pci, NULL);
} 

/* PCI IDs */
static struct pci_device_id snd_mychip_ids[] = {
        { PCI_VENDOR_ID_FOO, PCI_DEVICE_ID_BAR,PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0, },
        ....
        { 0, }
};
MODULE_DEVICE_TABLE(pci, snd_mychip_ids);

/* pci_driver definition */
static struct pci_driver driver = {
        .name = KBUILD_MODNAME,   //自行定义的名字
        .id_table = snd_mychip_ids,
        .probe = snd_mychip_probe,
        .remove = snd_mychip_remove,
};

/* module initialization */
static int __init alsa_card_mychip_init(void)
{
        return pci_register_driver(&driver);
}

/* module clean up */
static void __exit alsa_card_mychip_exit(void)
{
        pci_unregister_driver(&driver);
}

module_init(alsa_card_mychip_init)
module_exit(alsa_card_mychip_exit)
