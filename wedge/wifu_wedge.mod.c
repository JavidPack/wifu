#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x9a31bb74, "module_layout" },
	{ 0x522e98e9, "release_sock" },
	{ 0x5cd9dbb5, "kmalloc_caches" },
	{ 0xd2b09ce5, "__kmalloc" },
	{ 0x634eccda, "sock_init_data" },
	{ 0xf9a482f9, "msleep" },
	{ 0x716e7f27, "sock_release" },
	{ 0xf22449ae, "down_interruptible" },
	{ 0x1621c4d2, "kallsyms_on_each_symbol" },
	{ 0x91715312, "sprintf" },
	{ 0x4f8b5ddb, "_copy_to_user" },
	{ 0x12f854b4, "skb_queue_purge" },
	{ 0x2dab5fda, "sk_alloc" },
	{ 0x64ce4311, "current_task" },
	{ 0x27e1a049, "printk" },
	{ 0x6fd07228, "lock_sock_nested" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0x1e6d26a8, "strstr" },
	{ 0x34af0b52, "sk_free" },
	{ 0x42cab044, "netlink_unicast" },
	{ 0x44c87265, "init_net" },
	{ 0x6c2ef341, "proto_register" },
	{ 0x28ce48ff, "__alloc_skb" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x33cab908, "sock_register" },
	{ 0x62529e2a, "proto_unregister" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xd61adcbd, "kmem_cache_alloc_trace" },
	{ 0xd814baf, "__netlink_kernel_create" },
	{ 0x37a0cba, "kfree" },
	{ 0x69acdf38, "memcpy" },
	{ 0x62737e1d, "sock_unregister" },
	{ 0x71e3cecb, "up" },
	{ 0x4f6b400b, "_copy_from_user" },
	{ 0xcb7d430e, "__nlmsg_put" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "6405C5CE5A3CC3E14099D47");
