/**
 * MYTB ACPI Table Reader - 读取自定义 ACPI 表并通过 sysfs 展示
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("ACPI MYTB Table Reader");
MODULE_VERSION("1.0");

// 自定义 ACPI 表结构，必须与 UEFI 应用中定义的结构一致
struct mytb_table {
    struct acpi_table_header header;  // ACPI 表标准头部
    u32 data1;                        // 自定义数据字段
    u64 data2;                        // 自定义数据字段
    char message[32];                 // 自定义消息字段
} __packed;

// 全局变量保存表内容
static struct mytb_table *mytb = NULL;
static struct kobject *mytb_kobj = NULL;

// sysfs 属性显示函数
static ssize_t message_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (!mytb)
        return -EINVAL;

    return sprintf(buf, "%s\n", mytb->message);
}

static ssize_t data1_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (!mytb)
        return -EINVAL;

    return sprintf(buf, "0x%08X\n", mytb->data1);
}

static ssize_t data2_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (!mytb)
        return -EINVAL;

    return sprintf(buf, "0x%016llX\n", mytb->data2);
}

static ssize_t signature_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (!mytb)
        return -EINVAL;

    return sprintf(buf, "%.4s\n", mytb->header.signature);
}

// 定义 sysfs 属性
static struct kobj_attribute message_attr = 
    __ATTR(message, 0444, message_show, NULL);
static struct kobj_attribute data1_attr = 
    __ATTR(data1, 0444, data1_show, NULL);
static struct kobj_attribute data2_attr = 
    __ATTR(data2, 0444, data2_show, NULL);
static struct kobj_attribute signature_attr = 
    __ATTR(signature, 0444, signature_show, NULL);

// 属性数组
static struct attribute *mytb_attrs[] = {
    &message_attr.attr,
    &data1_attr.attr,
    &data2_attr.attr,
    &signature_attr.attr,
    NULL,
};

// 属性组
static struct attribute_group mytb_attr_group = {
    .attrs = mytb_attrs,
};

// ACPI 表处理函数
static int __init acpi_find_mytb_table(void)
{
    acpi_status status;
    struct acpi_table_header *table = NULL;
    
    // 查找 MYTB 签名的 ACPI 表
    status = acpi_get_table("MYTB", 0, &table);
    if (ACPI_FAILURE(status)) {
        pr_err("Failed to find MYTB ACPI table: %s\n", acpi_format_exception(status));
        return -ENODEV;
    }
    
    // 验证表大小
    if (table->length < sizeof(struct mytb_table)) {
        pr_err("MYTB table too small: %u bytes (expected at least %lu)\n", 
               table->length, sizeof(struct mytb_table));
        return -EINVAL;
    }
    
    // 保存表指针
    mytb = (struct mytb_table *)table;
    
    pr_info("MYTB ACPI table found\n");
    pr_info("  Signature: %.4s\n", mytb->header.signature);
    pr_info("  Length:    %u bytes\n", mytb->header.length);
    pr_info("  Data1:     0x%08X\n", mytb->data1);
    pr_info("  Data2:     0x%016llX\n", mytb->data2);
    pr_info("  Message:   %s\n", mytb->message);
    
    return 0;
}

static int __init mytb_reader_init(void)
{
    int ret;
    
    pr_info("MYTB ACPI Table Reader: initializing\n");
    
    // 查找 MYTB 表
    ret = acpi_find_mytb_table();
    if (ret) {
        pr_err("Failed to initialize MYTB reader\n");
        return ret;
    }
    
    // 创建 sysfs 目录
    mytb_kobj = kobject_create_and_add("mytb_acpi", kernel_kobj);
    if (!mytb_kobj) {
        pr_err("Failed to create sysfs kobject\n");
        return -ENOMEM;
    }
    
    // 创建 sysfs 属性文件
    ret = sysfs_create_group(mytb_kobj, &mytb_attr_group);
    if (ret) {
        pr_err("Failed to create sysfs attributes\n");
        kobject_put(mytb_kobj);
        return ret;
    }
    
    pr_info("MYTB ACPI Table Reader: initialized successfully\n");
    pr_info("Access MYTB table content at /sys/kernel/mytb_acpi/\n");
    
    return 0;
}

static void __exit mytb_reader_exit(void)
{
    if (mytb_kobj) {
        sysfs_remove_group(mytb_kobj, &mytb_attr_group);
        kobject_put(mytb_kobj);
    }
    
    pr_info("MYTB ACPI Table Reader: unloaded\n");
}

module_init(mytb_reader_init);
module_exit(mytb_reader_exit);
