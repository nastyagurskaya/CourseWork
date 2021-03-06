#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <asm/uaccess.h>

/*#define VENDOR_ID 0x1172
#define DEVICE_ID 0xe001*/
static int dev_open (struct inode *, struct file *);
static int dev_close (struct inode *, struct file *);
static ssize_t dev_read (struct file *, char *, size_t, loff_t *);
static ssize_t dev_write (struct file *, __user const char *, size_t, loff_t *);
static long dev_ioctl (struct file *file, unsigned int cmd,	unsigned long arg);

static struct pci_device_id ids[] = {
  { PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801AA_3), },
  { 0, }
};

MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, ids);
MODULE_AUTHOR("Nastya Gurskaya");
MODULE_VERSION("1");

int major = 60;

struct pci_dev *pdev;

struct priv_data {
	void* vaddr;
	dma_addr_t dma_handle;
};

static unsigned char get_revision(struct pci_dev *dev)
{
	u8 revision;
	pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
	return revision;
}

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	/* Do probing type stuff here.  
	 * Like calling request_region();
	 */
	printk("Probe\n");
	if(pci_enable_device(dev)) {
		dev_err(&dev->dev, "Can't enable PCI device\n");
		return -ENODEV;
	}	
	printk("Revision is %d\n",get_revision(dev));
	pci_set_master(dev);
	pdev = dev;
	
	return 0;
}

static void pci_remove(struct pci_dev *dev)
{
	/* clean up any allocated resources and stuff here.
	 * like call release_region();
	 */
	 printk("Remove\n"); 
}

static int dev_open (struct inode *inode, struct file *filp)
{
  struct priv_data *pd = kmalloc(sizeof(struct priv_data),GFP_DMA);
  void *vaddr;
  dma_addr_t dma_handle;
  
  printk("Open\n");
  printk("Allocate 0x%08lx\n",PAGE_SIZE);
  vaddr = dma_alloc_coherent(pdev,PAGE_SIZE,&dma_handle, GFP_KERNEL);
  printk("Virtual addr is 0x%016lx\n",(unsigned long)vaddr);
  printk("Bus addr is 0x%016lx\n",(unsigned long) dma_handle);

  pd->vaddr=vaddr;
  pd->dma_handle=dma_handle;
  filp->private_data = pd;
  return (0);
}

static int dev_close (struct inode *inode, struct file *filp)
{
  struct priv_data *pd = filp->private_data;
  printk("Close\n");
  dma_free_coherent(pdev,PAGE_SIZE,pd->vaddr,pd->dma_handle);
  kfree(filp->private_data);
  return (0);
}

static ssize_t dev_read (struct file * filp, char *buff, size_t count, loff_t * ppos)
{
  /* Transfering data to user space */ 
  unsigned long ul;

  struct priv_data *pd = filp->private_data;
  printk("Read count=0x%08lx\n",count);
  // copy_to_user returns bytes _not_ read
  ul = copy_to_user(buff,pd->vaddr,count);
  printk("Read 0x%08lx\n",count - ul);
  return count - ul;
  
}

static ssize_t dev_write (struct file * filp, __user const char *buff, size_t count, loff_t * ppos)
{
  // read from user space
  unsigned long ul;
  struct priv_data *pd = filp->private_data;
    printk("Write count=0x%08lx\n",count);
  // copy_from_user returns bytes _not_ write
  ul = copy_from_user(pd->vaddr,buff,count);
  printk("Write 0x%08lx\n",count - ul);
  return count - ul;
 
}

static long dev_ioctl (struct file *file, unsigned int cmd,	unsigned long arg){
	// return phys address
	struct priv_data *pd = file->private_data;
	printk("Ioctl cmd=%d\n",cmd);
	if (cmd == 1) {
		put_user(pd->dma_handle,(unsigned long*)arg);
	} 
	return 0;
}

static struct file_operations dev_fops =
{
  .owner = THIS_MODULE,
  .unlocked_ioctl = dev_ioctl,
  .open = dev_open,
  .release = dev_close,
  .read = dev_read,
  .write = dev_write,
  
};

static struct pci_driver pci_driver = {
	.name = "pci_bufmem",
	.id_table = ids,
	.probe = pci_probe,
	.remove = pci_remove,
};

static int __init pci_init(void)
{
	int rc;
	printk("init\n");
	rc = register_chrdev(major, "bufmem", &dev_fops);
	if ( rc < 0 ) {
		printk("cannot register chrdev\n");
		return rc;
	}
	printk("Register chrdev\n");
	rc = pci_register_driver(&pci_driver);
	if ( rc != 0 ){
		 unregister_chrdev(major,"bufmem");
		 printk("Cannot register pci\n");
		 return rc;
	}
	printk("Register pci\n");
	return rc;
}
	
static void __exit pci_exit(void)
{
	printk("exit\n");
	pci_unregister_driver(&pci_driver);
	unregister_chrdev(major,"bufmem");
}

module_init(pci_init);
module_exit(pci_exit);
