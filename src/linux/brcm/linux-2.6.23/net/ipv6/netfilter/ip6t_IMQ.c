/*
 * This target marks packets to be enqueued to an imq device
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_IMQ.h>
#include <linux/imq.h>

static unsigned int imq_target(struct sk_buff **pskb,
                              const struct net_device *in,
                              const struct net_device *out,
                              unsigned int hooknum,
			      const struct xt_target *target,
                              const void *targinfo)
{
       struct ip6t_imq_info *mr = (struct ip6t_imq_info*)targinfo;

       (*pskb)->imq_flags = mr->todev | IMQ_F_ENQUEUE;

       return IP6T_CONTINUE;
}

static int imq_checkentry(const char *tablename,
                         const void *e,
			 const struct xt_target *target,
                         void *targinfo,
                         unsigned int hook_mask)
{
       struct ip6t_imq_info *mr;

       mr = (struct ip6t_imq_info*)targinfo;

       if (strcmp(tablename, "mangle") != 0) {
               printk(KERN_WARNING
                      "IMQ: IMQ can only be called from \"mangle\" table, not \"%s\"\n",
                      tablename);
               return 0;
       }

       if (mr->todev > IMQ_MAX_DEVS) {
               printk(KERN_WARNING
                      "IMQ: invalid device specified, highest is %u\n",
                      IMQ_MAX_DEVS);
               return 0;
       }

       return 1;
}

static struct ip6t_target ip6t_imq_reg = {
       .name           = "IMQ",
       .target         = imq_target,
       .targetsize     = sizeof(struct ip6t_imq_info),
       .checkentry     = imq_checkentry,
       .me             = THIS_MODULE
};

static int __init init(void)
{
       if (ip6t_register_target(&ip6t_imq_reg))
               return -EINVAL;

       return 0;
}

static void __exit fini(void)
{
       ip6t_unregister_target(&ip6t_imq_reg);
}

module_init(init);
module_exit(fini);

MODULE_AUTHOR("http://www.linuximq.net");
MODULE_DESCRIPTION("Pseudo-driver for the intermediate queue device. See http://www.linuximq.net/ for more information.");
MODULE_LICENSE("GPL");
