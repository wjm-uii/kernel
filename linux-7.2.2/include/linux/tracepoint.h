/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_TRACEPOINT_H
#define _LINUX_TRACEPOINT_H

/*
 * Kernel Tracepoint API.
 *
 * See Documentation/trace/tracepoints.rst.
 *
 * Copyright (C) 2008-2014 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Heavily inspired from the Linux Kernel Markers.
 */

#include <linux/smp.h>
#include <linux/srcu.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/rcupdate.h>
#include <linux/rcupdate_trace.h>
#include <linux/tracepoint-defs.h>
#include <linux/static_call.h>
#include <linux/cfi.h>

struct module;
struct tracepoint;
struct notifier_block;

struct trace_eval_map {
	const char		*system;
	const char		*eval_string;
	unsigned long		eval_value;
};

#define TRACEPOINT_DEFAULT_PRIO	10

extern int
tracepoint_probe_register(struct tracepoint *tp, void *probe, void *data);
extern int
tracepoint_probe_register_prio(struct tracepoint *tp, void *probe, void *data,
			       int prio);
extern int
tracepoint_probe_register_prio_may_exist(struct tracepoint *tp, void *probe, void *data,
					 int prio);
extern int
tracepoint_probe_unregister(struct tracepoint *tp, void *probe, void *data);
static inline int
tracepoint_probe_register_may_exist(struct tracepoint *tp, void *probe,
				    void *data)
{
	return tracepoint_probe_register_prio_may_exist(tp, probe, data,
							TRACEPOINT_DEFAULT_PRIO);
}
extern void
for_each_kernel_tracepoint(void (*fct)(struct tracepoint *tp, void *priv),
		void *priv);

#ifdef CONFIG_MODULES
struct tp_module {
	struct list_head list;
	struct module *mod;
};

bool trace_module_has_bad_taint(struct module *mod);
extern int register_tracepoint_module_notifier(struct notifier_block *nb);
extern int unregister_tracepoint_module_notifier(struct notifier_block *nb);
void for_each_module_tracepoint(void (*fct)(struct tracepoint *,
					struct module *, void *),
				void *priv);
void for_each_tracepoint_in_module(struct module *,
				   void (*fct)(struct tracepoint *,
					struct module *, void *),
				   void *priv);
#else
static inline bool trace_module_has_bad_taint(struct module *mod)
{
	return false;
}
static inline
int register_tracepoint_module_notifier(struct notifier_block *nb)
{
	return 0;
}
static inline
int unregister_tracepoint_module_notifier(struct notifier_block *nb)
{
	return 0;
}
static inline
void for_each_module_tracepoint(void (*fct)(struct tracepoint *,
					struct module *, void *),
				void *priv)
{
}
static inline
void for_each_tracepoint_in_module(struct module *mod,
				   void (*fct)(struct tracepoint *,
					struct module *, void *),
				   void *priv)
{
}
#endif /* CONFIG_MODULES */

/*
 * tracepoint_synchronize_unregister must be called between the last tracepoint
 * probe unregistration and the end of module exit to make sure there is no
 * caller executing a probe when it is freed.
 *
 * An alternative is to use the following for batch reclaim associated
 * with a given tracepoint:
 *
 * - tracepoint_is_faultable() == false: call_srcu()
 * - tracepoint_is_faultable() == true:  call_rcu_tasks_trace()
 */
#ifdef CONFIG_TRACEPOINTS
extern struct srcu_struct tracepoint_srcu;
static inline void tracepoint_synchronize_unregister(void)
{
	synchronize_rcu_tasks_trace();
	synchronize_srcu(&tracepoint_srcu);
}
static inline bool tracepoint_is_faultable(struct tracepoint *tp)
{
	return tp->ext && tp->ext->faultable;
}
/*
 * Run RCU callback with the appropriate grace period wait for non-faultable
 * tracepoints, e.g., those used in atomic context.
 */
static inline void call_tracepoint_unregister_atomic(struct rcu_head *rcu, rcu_callback_t func)
{
	call_srcu(&tracepoint_srcu, rcu, func);
}
/*
 * Run RCU callback with the appropriate grace period wait for faultable
 * tracepoints, e.g., those used in syscall context.
 */
static inline void call_tracepoint_unregister_syscall(struct rcu_head *rcu, rcu_callback_t func)
{
	call_rcu_tasks_trace(rcu, func);
}
#else
static inline void tracepoint_synchronize_unregister(void)
{ }
static inline bool tracepoint_is_faultable(struct tracepoint *tp)
{
	return false;
}
static inline void call_tracepoint_unregister_atomic(struct rcu_head *rcu, rcu_callback_t func)
{  }
static inline void call_tracepoint_unregister_syscall(struct rcu_head *rcu, rcu_callback_t func)
{  }
#endif

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
extern int syscall_regfunc(void);
extern void syscall_unregfunc(void);
#endif /* CONFIG_HAVE_SYSCALL_TRACEPOINTS */

#ifndef PARAMS
#define PARAMS(args...) args
#endif

#define TRACE_DEFINE_ENUM(x)
#define TRACE_DEFINE_SIZEOF(x)

#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
static inline struct tracepoint *tracepoint_ptr_deref(tracepoint_ptr_t *p)
{
	return offset_to_ptr(p);
}

#define __TRACEPOINT_ENTRY(name)					\
	asm("	.section \"__tracepoints_ptrs\", \"a\"		\n"	\
	    "	.balign 4					\n"	\
	    "	.long 	__tracepoint_" #name " - .		\n"	\
	    "	.previous					\n")
#else
static inline struct tracepoint *tracepoint_ptr_deref(tracepoint_ptr_t *p)
{
	return *p;
}

#define __TRACEPOINT_ENTRY(name)					 \
	static tracepoint_ptr_t __tracepoint_ptr_##name __used		 \
	__section("__tracepoints_ptrs") = &__tracepoint_##name
#endif

#endif /* _LINUX_TRACEPOINT_H */

/*
 * Note: we keep the TRACE_EVENT and DECLARE_TRACE outside the include
 *  file ifdef protection.
 *  This is due to the way trace events work. If a file includes two
 *  trace event headers under one "CREATE_TRACE_POINTS" the first include
 *  will override the TRACE_EVENT and break the second include.
 */

#ifndef DECLARE_TRACE

#define TP_PROTO(args...)	args
#define TP_ARGS(args...)	args
#define TP_CONDITION(args...)	args

/*
 * Individual subsystem may have a separate configuration to
 * enable their tracepoints. By default, this file will create
 * the tracepoints if CONFIG_TRACEPOINTS is defined. If a subsystem
 * wants to be able to disable its tracepoints from being created
 * it can define NOTRACE before including the tracepoint headers.
 */
#if defined(CONFIG_TRACEPOINTS) && !defined(NOTRACE)
#define TRACEPOINTS_ENABLED
#endif

#ifdef TRACEPOINTS_ENABLED

#ifdef CONFIG_HAVE_STATIC_CALL
#define __DO_TRACE_CALL(name, args)					\
	do {								\
		struct tracepoint_func *it_func_ptr;			\
		void *__data;						\
		it_func_ptr =						\
			rcu_dereference_raw((&__tracepoint_##name)->funcs); \
		if (it_func_ptr) {					\
			__data = (it_func_ptr)->data;			\
			static_call(tp_func_##name)(__data, args);	\
		}							\
	} while (0)
#else
#define __DO_TRACE_CALL(name, args)	__traceiter_##name(NULL, args)
#endif /* CONFIG_HAVE_STATIC_CALL */

/*
 * Declare an exported function that Rust code can call to trigger this
 * tracepoint. This function does not include the static branch; that is done
 * in Rust to avoid a function call when the tracepoint is disabled.
 */
#define DEFINE_RUST_DO_TRACE(name, proto, args)
#define __DEFINE_RUST_DO_TRACE(name, proto, args)			\
	notrace void rust_do_trace_##name(proto)			\
	{								\
		__do_trace_##name(args);				\
	}

/*
 * When a tracepoint is used, it's name is added to the __tracepoint_check
 * section. This section is only used at build time to make sure all
 * defined tracepoints are used. It is discarded after the build.
 */
# define TRACEPOINT_CHECK(name)						\
	static const char __used __section("__tracepoint_check")	\
	__trace_check_##name[] = #name;

/*
 * Make sure the alignment of the structure in the __tracepoints section will
 * not add unwanted padding between the beginning of the section and the
 * structure. Force alignment to the same alignment as the section start.
 *
 * When lockdep is enabled, we make sure to always test if RCU is
 * "watching" regardless if the tracepoint is enabled or not. Tracepoints
 * require RCU to be active, and it should always warn at the tracepoint
 * site if it is not watching, as it will need to be active when the
 * tracepoint is enabled.
 */
#define __DECLARE_TRACE_COMMON(name, proto, args, data_proto)		\
	extern int __traceiter_##name(data_proto);			\
	DECLARE_STATIC_CALL(tp_func_##name, __traceiter_##name);	\
	extern struct tracepoint __tracepoint_##name;			\
	extern void rust_do_trace_##name(proto);			\
	static inline int						\
	register_trace_##name(void (*probe)(data_proto), void *data)	\
	{								\
		return tracepoint_probe_register(&__tracepoint_##name,	\
						(void *)probe, data);	\
	}								\
	static inline int						\
	register_trace_prio_##name(void (*probe)(data_proto), void *data,\
				   int prio)				\
	{								\
		return tracepoint_probe_register_prio(&__tracepoint_##name, \
					      (void *)probe, data, prio); \
	}								\
	static inline int						\
	unregister_trace_##name(void (*probe)(data_proto), void *data)	\
	{								\
		return tracepoint_probe_unregister(&__tracepoint_##name,\
						(void *)probe, data);	\
	}								\
	static inline void						\
	check_trace_callback_type_##name(void (*cb)(data_proto))	\
	{								\
	}								\
	static inline bool						\
	__trace_##name##_enabled(void)					\
	{								\
		return static_branch_unlikely(&__tracepoint_##name.key);\
	}								\
	static inline bool						\
	trace_##name##_enabled(void)					\
	{								\
		if (IS_ENABLED(CONFIG_LOCKDEP)) {			\
			WARN_ONCE(!rcu_is_watching(),			\
				  "RCU not watching for tracepoint");	\
		}							\
		return __trace_##name##_enabled();			\
	}

#define __DECLARE_TRACE(name, proto, args, cond, data_proto)			\
	__DECLARE_TRACE_COMMON(name, PARAMS(proto), PARAMS(args), PARAMS(data_proto)) \
	static inline void __do_trace_##name(proto)			\
	{								\
		TRACEPOINT_CHECK(name)					\
		if (cond) {						\
			guard(srcu_fast_notrace)(&tracepoint_srcu);	\
			__DO_TRACE_CALL(name, TP_ARGS(args));		\
		}							\
	}								\
	static inline void trace_##name(proto)				\
	{								\
		if (static_branch_unlikely(&__tracepoint_##name.key))	\
			__do_trace_##name(args);			\
		if (IS_ENABLED(CONFIG_LOCKDEP) && (cond)) {		\
			WARN_ONCE(!rcu_is_watching(),			\
				  "RCU not watching for tracepoint");	\
		}							\
	}								\
	static inline void trace_call__##name(proto)			\
	{								\
		__do_trace_##name(args);				\
	}
/*
后续中__DECLARE_TRACE_COMMON的name参数会被rust_do_trace_##name(proto)调用
static inline void __do_trace_##name(proto)是name函数的实现
__DECLARE_TRACE_COMMON(name, PARAMS(proto), PARAMS(args), PARAMS(data_proto))各个参数的解释：
1. name（跟踪点标识符）
含义：Tracepoint 的原始名称（例如 pelt_cfs）。
作用：提供给预处理器，利用 ## 记号拼接生成所有相关的符号名（如 __tracepoint_pelt_cfs、register_trace_pelt_cfs、__traceiter_pelt_cfs 等）。
2. PARAMS(proto)（事件触发函数的形参声明）
含义：触发 Tracepoint 时使用的 C 语言函数参数原型列表（包含类型和形参名）。
作用：决定内核业务代码在调用 trace_##name(...) 或 rust_do_trace_##name(...) 时的入参形式。
3. PARAMS(args)（事件触发函数的实参传递列表）
含义：调用 Tracepoint 时传递给底层逻辑的变量名（只包含变量名，不含类型）。
作用：在 trace_##name 内部转发参数时使用，例如将接收到的变量直接传递给 __do_trace_##name(args)。
4. PARAMS(data_proto)（探针回调函数的形参声明）
含义：外部模块注册的探针（Probe）回调函数的完整参数签名。
核心区别：比 proto 在最前面固定多出了一个私有数据指针（void *__data）。
作用：决定外部注册函数 register_trace_##name 以及迭代器 __traceiter_##name 接受的回调函数指针类型（void (*probe)(data_proto)）。

宏参数		代入后的实际值		最终生成的 C 代码片段
name	pelt_cfs	生成符号 extern struct tracepoint __tracepoint_pelt_cfs;
proto	struct cfs_rq *cfs_rq	生成函数 extern void rust_do_trace_pelt_cfs(struct cfs_rq *cfs_rq);
args	cfs_rq		用于 trace_pelt_cfs 内部调用：__do_trace_pelt_cfs(cfs_rq);
data_proto		void *__data, struct cfs_rq *cfs_rq		生成注册 API：register_trace_pelt_cfs(void (*probe)(void *__data, struct cfs_rq *cfs_rq), void *data)

为什么使用inline
确实如此，static inline 是这套“零开销跟踪点”机制的立足之本。
如果不用 inline，这种拆分设计不仅无法发挥作用，反而会导致整个 Tracepoint 架构的性能优势荡然无存。
不用 inline 会发生什么？
1.破坏“零开销（Zero-Overhead）”承诺
如果 trace_##name 是一个普通函数（非 inline），每次代码运行到触发点时，CPU 都必须执行：
参数准备 -> 寄存器压栈 -> CALL 跳转 -> 函数内部判断开闭状态 -> RET 返回。
即使 Tracepoint 处于关闭状态，这套函数调用开销也已经产生了。
对于调度器等每秒执行数百万次的热点代码来说，这种固定损耗是不可接受的。
2.引发头文件多重定义冲突（ODR 违反）
DECLARE_TRACE 宏展开的代码是写在 .h 头文件里的。如果不加 static inline，当这个头文件被多个 .c 
文件同时 #include 时，编译链接阶段会直接报 multiple definition（符号重复定义）错误。
inline 与“函数拆分”是如何配合的？
既然 trace_##name 和 __do_trace_##name 都带有 inline，为什么不直接写成一个函数？这利用了编译器的冷热路径优化（Hot/Cold Code Splitting）：
热路径（Fast Path）极致纤薄：通过 inline，编译器把 trace_##name 中的 if (static_branch_unlikely(...)) 逻辑直接打碎并嵌入到调用者函数（如 update_rt_rq_load）内部。
当 Tracepoint 未开启时，汇编层仅仅是一条 NOP（空指令），CPU 瞬间穿过，没有任何函数调用开销。
冷路径（Slow Path）隔离隔离：当编译器看到 static_branch_unlikely（分支预测概率极低）以及被拆分出去的 __do_trace_##name 时，
它会智能地把 __do_trace_##name 展开后的那一大堆复杂代码（SRCU 加锁、参数打包、回调遍历）推到汇编代码的最末端，
甚至不予内联。这样既保证了调用者主函数的指令缓存（I-Cache）不被污染，又防止了主函数因为局部变量过多而破坏寄存器分配。
总结
static inline 解决了“如何在头文件定义”以及“关闭时实现零函数调用开销”的问题；
拆分出 __do_trace 解决了“开启时的复杂逻辑不干扰主函数 I-Cache 和寄存器”的问题。两者缺一不可。

__do_trace_##name的解释
__do_trace_##name 中的 ## 是 C 语言预处理器的 Token 拼接运算符（记号连接符）。
它的作用是在编译前的预处理阶段，把前缀字符串与传入的参数 name 直接硬黏合成一个新的函数名。
拼接代入过程（以 name = pelt_cfs 为例）
当宏参数 name 传入 pelt_cfs 时，预处理器在处理 ##name 时会进行文本替换
宏内部写法	## 拼接后的实际 C 代码标识符	真实角色
__tracepoint_##name	__tracepoint_pelt_cfs	元数据结构体变量
trace_##name	trace_pelt_cfs	给业务代码调用的触发入口
__do_trace_##name	__do_trace_pelt_cfs	内部真正执行回调的私有函数
trace_call__##name	trace_call__pelt_cfs	强制调用的底层入口
register_trace_##name	register_trace_pelt_cfs	给外部模块调用的注册函数


__DECLARE_TRACE(name, proto, args, cond, data_proto) 的name，怎么和static inline void __do_trace_##name的name相连的，是一种什么用法
这种用法被称为 C 语言预处理器的“记号连接运算符”（Token Pasting Operator，即 ## 运算符）。
它是一种在 C 语言中用来实现元编程（代码自动生成/伪模板）的核心技巧。
1. 它是怎么连接的？（预处理器的“剪贴”过程）当编译预处理器处理代码时，它会将 ## 两边的文本强制黏合成一个新的 C 语言标识符（函数名、变量名或结构体名）。
假设你调用了宏：__DECLARE_TRACE(pelt_cfs, ...)，此时 name 的值是 pelt_cfs。
预处理器的处理步骤如下：
1.识别参数：找到宏内部所有的 name，准备用 pelt_cfs 替换。
2.切分 Token：看到 __do_trace_##name，预处理器识别出左边是静态文本 __do_trace_，右边是变量 name。
3.替换与拼接：将 name 替换为 pelt_cfs -> 变成 __do_trace_ 和 pelt_cfs。执行 ## 运算，将两者强行无缝贴合->变成 __do_trace_pelt_cfs。
4.生成最终函数：Cstatic inline void __do_trace_pelt_cfs(struct cfs_rq *cfs_rq)


那么pelt_cfs是怎么和pelt_cfs_tp联合在一起的

pelt_cfs 与 pelt_cfs_tp 的“联合”本质上是功能分工与导出封装：前者面向系统级 Ftrace 工具，
后者作为 Bare Tracepoint（裸跟踪点） 专门导出给外部 GPL 模块使用。
两者的代码分布与联合机制如下：
代码分布位置
1. 声明位置（Header）
在 include/trace/events/sched.h 中，内核分别对两者进行了声明：
1. 普通 Trace Event（供 ftrace 使用）
TRACE_EVENT(pelt_cfs,
    TP_PROTO(struct cfs_rq *cfs_rq),
    ...
);

2. Bare Tracepoint（裸跟踪点，专门用于导出给模块）
DECLARE_TRACE(pelt_cfs_tp,
    TP_PROTO(struct cfs_rq *cfs_rq),
    TP_ARGS(cfs_rq)
);

2. 实例化与符号导出（Core）
在 kernel/sched/core.c 中，内核对 pelt_cfs_tp 进行了实例化并向外导出符号：
在 core.c 中分配元数据结构体变量 __tracepoint_pelt_cfs_tp 
DEFINE_TRACE(pelt_cfs_tp);

将符号导出给外部 GPL 内核模块 
EXPORT_TRACEPOINT_SYMBOL_GPL(pelt_cfs_tp);
3. 业务触发（Trigger）
在 CFS 调度器更新负载的逻辑文件 kernel/sched/fair.c 
或 kernel/sched/pelt.c 中（例如 update_cfs_rq_load_avg() 函数）：
1. 触发 ftrace 事件
trace_pelt_cfs(cfs_rq);

2. 同时触发导出给外部模块的裸跟踪点 
trace_pelt_cfs_tp(cfs_rq);

两者联合的底层机制
共享相同的数据源：在 CFS 调度器更新 cfs_rq（公平调度运行队列）的 PELT 负载统计时，
主干代码会将当前的 struct cfs_rq *cfs_rq 指针作为参数同时透传给这两个跟踪点。
隔离 Ftrace 性能开销：pelt_cfs 绑定了 Ftrace 的环形缓冲区（Ring Buffer）格式化逻辑
（用于 /sys/kernel/tracing/events/sched/pelt_cfs）；而 pelt_cfs_tp 不含任何格式化开销，
外部模块（如厂商调频驱动、eBPF 探针）注册它时，可以直接拿到原始的 struct cfs_rq 指针并高效读取数据。
_tp 后缀的作用：内核社区在重构调度器跟踪点时，统一给专门用来 EXPORT_TRACEPOINT_SYMBOL_GPL 的 
Bare Tracepoint 加上了 _tp 后缀，用于和 sysfs 中的 Ftrace 事件名做明确区分。

*/
#define __DECLARE_TRACE_SYSCALL(name, proto, args, data_proto)		\
	__DECLARE_TRACE_COMMON(name, PARAMS(proto), PARAMS(args), PARAMS(data_proto)) \
	static inline void __do_trace_##name(proto)			\
	{								\
		TRACEPOINT_CHECK(name)					\
		guard(rcu_tasks_trace)();				\
		__DO_TRACE_CALL(name, TP_ARGS(args));			\
	}								\
	static inline void trace_##name(proto)				\
	{								\
		might_fault();						\
		if (static_branch_unlikely(&__tracepoint_##name.key))	\
			__do_trace_##name(args);			\
		if (IS_ENABLED(CONFIG_LOCKDEP)) {			\
			WARN_ONCE(!rcu_is_watching(),			\
				  "RCU not watching for tracepoint");	\
		}							\
	}								\
	static inline void trace_call__##name(proto)			\
	{								\
		might_fault();						\
		__do_trace_##name(args);				\
	}

/*
 * We have no guarantee that gcc and the linker won't up-align the tracepoint
 * structures, so we create an array of pointers that will be used for iteration
 * on the tracepoints.
 *
 * it_func[0] is never NULL because there is at least one element in the array
 * when the array itself is non NULL.
 */
#define __DEFINE_TRACE_EXT(_name, _ext, proto, args)			\
	static const char __tpstrtab_##_name[]				\
	__section("__tracepoints_strings") = #_name;			\
	extern struct static_call_key STATIC_CALL_KEY(tp_func_##_name);	\
	int __traceiter_##_name(void *__data, proto);			\
	void __probestub_##_name(void *__data, proto);			\
	struct tracepoint __tracepoint_##_name	__used			\
	__section("__tracepoints") = {					\
		.name = __tpstrtab_##_name,				\
		.key = STATIC_KEY_FALSE_INIT,				\
		.static_call_key = &STATIC_CALL_KEY(tp_func_##_name),	\
		.static_call_tramp = STATIC_CALL_TRAMP_ADDR(tp_func_##_name), \
		.iterator = &__traceiter_##_name,			\
		.probestub = &__probestub_##_name,			\
		.funcs = NULL,						\
		.ext = _ext,						\
	};								\
	__TRACEPOINT_ENTRY(_name);					\
	int __traceiter_##_name(void *__data, proto)			\
	{								\
		struct tracepoint_func *it_func_ptr;			\
		void *it_func;						\
									\
		it_func_ptr =						\
			rcu_dereference_raw((&__tracepoint_##_name)->funcs); \
		if (it_func_ptr) {					\
			do {						\
				it_func = READ_ONCE((it_func_ptr)->func); \
				__data = (it_func_ptr)->data;		\
				((void(*)(void *, proto))(it_func))(__data, args); \
			} while ((++it_func_ptr)->func);		\
		}							\
		return 0;						\
	}								\
	void __probestub_##_name(void *__data, proto)			\
	{								\
	}								\
	/*								\
	 * Annotate the probestub 'CFI_NOSEAL' to stop objtool from	\
	 * requesting the kernel remove the ENDBR, because the only	\
	 * references to the function are in the __tracepoint section,	\
	 * that objtool doesn't scan.					\
	 */								\
	CFI_NOSEAL(__probestub_##_name);				\
	DEFINE_STATIC_CALL(tp_func_##_name, __traceiter_##_name);	\
	DEFINE_RUST_DO_TRACE(_name, TP_PROTO(proto), TP_ARGS(args))

#define DEFINE_TRACE_FN(_name, _reg, _unreg, _proto, _args)		\
	static struct tracepoint_ext __tracepoint_ext_##_name = {	\
		.regfunc = _reg,					\
		.unregfunc = _unreg,					\
		.faultable = false,					\
	};								\
	__DEFINE_TRACE_EXT(_name, &__tracepoint_ext_##_name, PARAMS(_proto), PARAMS(_args));

#define DEFINE_TRACE_SYSCALL(_name, _reg, _unreg, _proto, _args)	\
	static struct tracepoint_ext __tracepoint_ext_##_name = {	\
		.regfunc = _reg,					\
		.unregfunc = _unreg,					\
		.faultable = true,					\
	};								\
	__DEFINE_TRACE_EXT(_name, &__tracepoint_ext_##_name, PARAMS(_proto), PARAMS(_args));

#define DEFINE_TRACE(_name, _proto, _args)				\
	__DEFINE_TRACE_EXT(_name, NULL, PARAMS(_proto), PARAMS(_args));

/*
维度		DECLARE_TRACE(name, proto, args)		DEFINE_TRACE(name, proto, args)
位置		头文件 (.h)								源文件 (.c / define_trace.h)
本质	声明（纯代码生成）	定义（真正分配内存）
展开结果	extern 结构体 + static inline 触发/注册 API		全局 struct tracepoint 变量 + __traceiter 实体函数
内存占用	0 字节（仅头文件内联逻辑）	占用真实的 kernel data/text 段内存
调用次数	任意 .c 文件包含多次	全局有且仅有 1 次
*/

#define EXPORT_TRACEPOINT_SYMBOL_GPL(name)				\
	TRACEPOINT_CHECK(name)						\
	EXPORT_SYMBOL_GPL(__tracepoint_##name);				\
	EXPORT_SYMBOL_GPL(__traceiter_##name);				\
	EXPORT_STATIC_CALL_GPL(tp_func_##name)
#define EXPORT_TRACEPOINT_SYMBOL(name)					\
	TRACEPOINT_CHECK(name)						\
	EXPORT_SYMBOL(__tracepoint_##name);				\
	EXPORT_SYMBOL(__traceiter_##name);				\
	EXPORT_STATIC_CALL(tp_func_##name)


#else /* !TRACEPOINTS_ENABLED */
#define __DECLARE_TRACE_COMMON(name, proto, args, data_proto)		\
	static inline void trace_##name(proto)				\
	{ }								\
	static inline void trace_call__##name(proto)			\
	{ }								\
	static inline int						\
	register_trace_##name(void (*probe)(data_proto),		\
			      void *data)				\
	{								\
		return -ENOSYS;						\
	}								\
	static inline int						\
	unregister_trace_##name(void (*probe)(data_proto),		\
				void *data)				\
	{								\
		return -ENOSYS;						\
	}								\
	static inline void check_trace_callback_type_##name(void (*cb)(data_proto)) \
	{								\
	}								\
	static inline bool						\
	__trace_##name##_enabled(void)					\
	{								\
		return false;						\
	}								\
	static inline bool						\
	trace_##name##_enabled(void)					\
	{								\
		return false;						\
	}

#define __DECLARE_TRACE(name, proto, args, cond, data_proto)		\
	__DECLARE_TRACE_COMMON(name, PARAMS(proto), PARAMS(args), PARAMS(data_proto))

#define __DECLARE_TRACE_SYSCALL(name, proto, args, data_proto)		\
	__DECLARE_TRACE_COMMON(name, PARAMS(proto), PARAMS(args), PARAMS(data_proto))

#define DEFINE_TRACE_FN(name, reg, unreg, proto, args)
#define DEFINE_TRACE_SYSCALL(name, reg, unreg, proto, args)
#define DEFINE_TRACE(name, proto, args)
#define EXPORT_TRACEPOINT_SYMBOL_GPL(name)
#define EXPORT_TRACEPOINT_SYMBOL(name)

#endif /* TRACEPOINTS_ENABLED */

#ifdef CONFIG_TRACING
/**
 * tracepoint_string - register constant persistent string to trace system
 * @str - a constant persistent string that will be referenced in tracepoints
 *
 * If constant strings are being used in tracepoints, it is faster and
 * more efficient to just save the pointer to the string and reference
 * that with a printf "%s" instead of saving the string in the ring buffer
 * and wasting space and time.
 *
 * The problem with the above approach is that userspace tools that read
 * the binary output of the trace buffers do not have access to the string.
 * Instead they just show the address of the string which is not very
 * useful to users.
 *
 * With tracepoint_string(), the string will be registered to the tracing
 * system and exported to userspace via the debugfs/tracing/printk_formats
 * file that maps the string address to the string text. This way userspace
 * tools that read the binary buffers have a way to map the pointers to
 * the ASCII strings they represent.
 *
 * The @str used must be a constant string and persistent as it would not
 * make sense to show a string that no longer exists. But it is still fine
 * to be used with modules, because when modules are unloaded, if they
 * had tracepoints, the ring buffers are cleared too. As long as the string
 * does not change during the life of the module, it is fine to use
 * tracepoint_string() within a module.
 */
#define tracepoint_string(str)						\
	({								\
		static const char *___tp_str __tracepoint_string = str; \
		___tp_str;						\
	})
#define __tracepoint_string	__used __section("__tracepoint_str")
#else
/*
 * tracepoint_string() is used to save the string address for userspace
 * tracing tools. When tracing isn't configured, there's no need to save
 * anything.
 */
# define tracepoint_string(str) str
# define __tracepoint_string
#endif

#define DECLARE_TRACE(name, proto, args)				\
	__DECLARE_TRACE(name##_tp, PARAMS(proto), PARAMS(args),		\
			cpu_online(raw_smp_processor_id()),		\
			PARAMS(void *__data, proto))

#define DECLARE_TRACE_CONDITION(name, proto, args, cond)		\
	__DECLARE_TRACE(name##_tp, PARAMS(proto), PARAMS(args),		\
			cpu_online(raw_smp_processor_id()) && (PARAMS(cond)), \
			PARAMS(void *__data, proto))

#define DECLARE_TRACE_SYSCALL(name, proto, args)			\
	__DECLARE_TRACE_SYSCALL(name##_tp, PARAMS(proto), PARAMS(args),	\
				PARAMS(void *__data, proto))

#define DECLARE_TRACE_EVENT(name, proto, args)				\
	__DECLARE_TRACE(name, PARAMS(proto), PARAMS(args),		\
			cpu_online(raw_smp_processor_id()),		\
			PARAMS(void *__data, proto))

#define DECLARE_TRACE_EVENT_CONDITION(name, proto, args, cond)		\
	__DECLARE_TRACE(name, PARAMS(proto), PARAMS(args),		\
			cpu_online(raw_smp_processor_id()) && (PARAMS(cond)), \
			PARAMS(void *__data, proto))

#define DECLARE_TRACE_EVENT_SYSCALL(name, proto, args)			\
	__DECLARE_TRACE_SYSCALL(name, PARAMS(proto), PARAMS(args),	\
				PARAMS(void *__data, proto))

#define TRACE_EVENT_FLAGS(event, flag)

#define TRACE_EVENT_PERF_PERM(event, expr...)

#endif /* DECLARE_TRACE */

#ifndef TRACE_EVENT
/*
 * For use with the TRACE_EVENT macro:
 *
 * We define a tracepoint, its arguments, its printk format
 * and its 'fast binary record' layout.
 *
 * Firstly, name your tracepoint via TRACE_EVENT(name : the
 * 'subsystem_event' notation is fine.
 *
 * Think about this whole construct as the
 * 'trace_sched_switch() function' from now on.
 *
 *
 *  TRACE_EVENT(sched_switch,
 *
 *	*
 *	* A function has a regular function arguments
 *	* prototype, declare it via TP_PROTO():
 *	*
 *
 *	TP_PROTO(struct rq *rq, struct task_struct *prev,
 *		 struct task_struct *next),
 *
 *	*
 *	* Define the call signature of the 'function'.
 *	* (Design sidenote: we use this instead of a
 *	*  TP_PROTO1/TP_PROTO2/TP_PROTO3 ugliness.)
 *	*
 *
 *	TP_ARGS(rq, prev, next),
 *
 *	*
 *	* Fast binary tracing: define the trace record via
 *	* TP_STRUCT__entry(). You can think about it like a
 *	* regular C structure local variable definition.
 *	*
 *	* This is how the trace record is structured and will
 *	* be saved into the ring buffer. These are the fields
 *	* that will be exposed to user-space in
 *	* /sys/kernel/tracing/events/<*>/format.
 *	*
 *	* The declared 'local variable' is called '__entry'
 *	*
 *	* __field(pid_t, prev_pid) is equivalent to a standard declaration:
 *	*
 *	*	pid_t	prev_pid;
 *	*
 *	* __array(char, prev_comm, TASK_COMM_LEN) is equivalent to:
 *	*
 *	*	char	prev_comm[TASK_COMM_LEN];
 *	*
 *
 *	TP_STRUCT__entry(
 *		__array(	char,	prev_comm,	TASK_COMM_LEN	)
 *		__field(	pid_t,	prev_pid			)
 *		__field(	int,	prev_prio			)
 *		__array(	char,	next_comm,	TASK_COMM_LEN	)
 *		__field(	pid_t,	next_pid			)
 *		__field(	int,	next_prio			)
 *	),
 *
 *	*
 *	* Assign the entry into the trace record, by embedding
 *	* a full C statement block into TP_fast_assign(). You
 *	* can refer to the trace record as '__entry' -
 *	* otherwise you can put arbitrary C code in here.
 *	*
 *	* Note: this C code will execute every time a trace event
 *	* happens, on an active tracepoint.
 *	*
 *
 *	TP_fast_assign(
 *		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
 *		__entry->prev_pid	= prev->pid;
 *		__entry->prev_prio	= prev->prio;
 *		memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
 *		__entry->next_pid	= next->pid;
 *		__entry->next_prio	= next->prio;
 *	),
 *
 *	*
 *	* Formatted output of a trace record via TP_printk().
 *	* This is how the tracepoint will appear under ftrace
 *	* plugins that make use of this tracepoint.
 *	*
 *	* (raw-binary tracing wont actually perform this step.)
 *	*
 *
 *	TP_printk("task %s:%d [%d] ==> %s:%d [%d]",
 *		__entry->prev_comm, __entry->prev_pid, __entry->prev_prio,
 *		__entry->next_comm, __entry->next_pid, __entry->next_prio),
 *
 * );
 *
 * This macro construct is thus used for the regular printk format
 * tracing setup, it is used to construct a function pointer based
 * tracepoint callback (this is used by programmatic plugins and
 * can also by used by generic instrumentation like SystemTap), and
 * it is also used to expose a structured trace record in
 * /sys/kernel/tracing/events/.
 *
 * A set of (un)registration functions can be passed to the variant
 * TRACE_EVENT_FN to perform any (un)registration work.
 */

#define DECLARE_EVENT_CLASS(name, proto, args, tstruct, assign, print)
#define DEFINE_EVENT(template, name, proto, args)		\
	DECLARE_TRACE_EVENT(name, PARAMS(proto), PARAMS(args))
#define DEFINE_EVENT_FN(template, name, proto, args, reg, unreg)\
	DECLARE_TRACE_EVENT(name, PARAMS(proto), PARAMS(args))
#define DEFINE_EVENT_PRINT(template, name, proto, args, print)	\
	DECLARE_TRACE_EVENT(name, PARAMS(proto), PARAMS(args))
#define DEFINE_EVENT_CONDITION(template, name, proto,		\
			       args, cond)			\
	DECLARE_TRACE_EVENT_CONDITION(name, PARAMS(proto),	\
				PARAMS(args), PARAMS(cond))

#define TRACE_EVENT(name, proto, args, struct, assign, print)	\
	DECLARE_TRACE_EVENT(name, PARAMS(proto), PARAMS(args))
#define TRACE_EVENT_FN(name, proto, args, struct,		\
		assign, print, reg, unreg)			\
	DECLARE_TRACE_EVENT(name, PARAMS(proto), PARAMS(args))
#define TRACE_EVENT_FN_COND(name, proto, args, cond, struct,	\
		assign, print, reg, unreg)			\
	DECLARE_TRACE_EVENT_CONDITION(name, PARAMS(proto),	\
			PARAMS(args), PARAMS(cond))
#define TRACE_EVENT_CONDITION(name, proto, args, cond,		\
			      struct, assign, print)		\
	DECLARE_TRACE_EVENT_CONDITION(name, PARAMS(proto),	\
				PARAMS(args), PARAMS(cond))
#define TRACE_EVENT_SYSCALL(name, proto, args, struct, assign,	\
			    print, reg, unreg)			\
	DECLARE_TRACE_EVENT_SYSCALL(name, PARAMS(proto), PARAMS(args))

#define TRACE_EVENT_FLAGS(event, flag)

#define TRACE_EVENT_PERF_PERM(event, expr...)

#define DECLARE_EVENT_NOP(name, proto, args)				\
	static inline void trace_##name(proto)				\
	{ }								\
	static inline bool trace_##name##_enabled(void)			\
	{								\
		return false;						\
	}

#define TRACE_EVENT_NOP(name, proto, args, struct, assign, print)	\
	DECLARE_EVENT_NOP(name, PARAMS(proto), PARAMS(args))

#define DECLARE_EVENT_CLASS_NOP(name, proto, args, tstruct, assign, print)
#define DEFINE_EVENT_NOP(template, name, proto, args)			\
	DECLARE_EVENT_NOP(name, PARAMS(proto), PARAMS(args))

#endif /* ifdef TRACE_EVENT (see note above) */
