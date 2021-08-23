/* originally from linux source.
 * removed the dependencies on CONFIG_ values
 * removed virt_to_phys stuff (and in fact everything surrounded by #if __KERNEL__)
 * Modified By Rob Taylor, Flying Pig Systems, 2000
 */

#ifndef _PPC_IO_H
#define _PPC_IO_H

#include <asm/byteorder.h>

#define SIO_CONFIG_RA   0x398
#define SIO_CONFIG_RD   0x399

#define _IO_BASE	0

#define readb(addr) in_8((volatile u8 *)(addr))
#define writeb(b,addr) out_8((volatile u8 *)(addr), (b))
#if !defined(__BIG_ENDIAN)
#define readw(addr) (*(volatile u16 *) (addr))
#define readl(addr) (*(volatile u32 *) (addr))
#define writew(b,addr) ((*(volatile u16 *) (addr)) = (b))
#define writel(b,addr) ((*(volatile u32 *) (addr)) = (b))
#else
#define readw(addr) in_le16((volatile u16 *)(addr))
#define readl(addr) in_le32((volatile u32 *)(addr))
#define writew(b,addr) out_le16((volatile u16 *)(addr),(b))
#define writel(b,addr) out_le32((volatile u32 *)(addr),(b))
#endif

/*
 * The insw/outsw/insl/outsl macros don't do byte-swapping.
 * They are only used in practice for transferring buffers which
 * are arrays of bytes, and byte-swapping is not appropriate in
 * that case.  - paulus
 */
#define insb(port, buf, ns) _insb((u8 *)((port)+_IO_BASE), (buf), (ns))
#define outsb(port, buf, ns)    _outsb((u8 *)((port)+_IO_BASE), (buf), (ns))
#define insw(port, buf, ns) _insw_ns((u16 *)((port)+_IO_BASE), (buf), (ns))
#define outsw(port, buf, ns)    _outsw_ns((u16 *)((port)+_IO_BASE), (buf), (ns))
#define insl(port, buf, nl) _insl_ns((u32 *)((port)+_IO_BASE), (buf), (nl))
#define outsl(port, buf, nl)    _outsl_ns((u32 *)((port)+_IO_BASE), (buf), (nl))

#define inb(port)       in_8((u8 *)((port)+_IO_BASE))
#define outb(val, port)     out_8((u8 *)((port)+_IO_BASE), (val))
#if !defined(__BIG_ENDIAN)
#define inw(port)       in_be16((u16 *)((port)+_IO_BASE))
#define outw(val, port)     out_be16((u16 *)((port)+_IO_BASE), (val))
#define inl(port)       in_be32((u32 *)((port)+_IO_BASE))
#define outl(val, port)     out_be32((u32 *)((port)+_IO_BASE), (val))
#else
#define inw(port)       in_le16((u16 *)((port)+_IO_BASE))
#define outw(val, port)     out_le16((u16 *)((port)+_IO_BASE), (val))
#define inl(port)       in_le32((u32 *)((port)+_IO_BASE))
#define outl(val, port)     out_le32((u32 *)((port)+_IO_BASE), (val))
#endif

#define inb_p(port)     in_8((u8 *)((port)+_IO_BASE))
#define outb_p(val, port)   out_8((u8 *)((port)+_IO_BASE), (val))
#define inw_p(port)     in_le16((u16 *)((port)+_IO_BASE))
#define outw_p(val, port)   out_le16((u16 *)((port)+_IO_BASE), (val))
#define inl_p(port)     in_le32((u32 *)((port)+_IO_BASE))
#define outl_p(val, port)   out_le32((u32 *)((port)+_IO_BASE), (val))

extern void _insb(volatile u8 *port, void *buf, int ns);
extern void _outsb(volatile u8 *port, const void *buf, int ns);
extern void _insw(volatile u16 *port, void *buf, int ns);
extern void _outsw(volatile u16 *port, const void *buf, int ns);
extern void _insl(volatile u32 *port, void *buf, int nl);
extern void _outsl(volatile u32 *port, const void *buf, int nl);
extern void _insw_ns(volatile u16 *port, void *buf, int ns);
extern void _outsw_ns(volatile u16 *port, const void *buf, int ns);
extern void _insl_ns(volatile u32 *port, void *buf, int nl);
extern void _outsl_ns(volatile u32 *port, const void *buf, int nl);

/*
 * The *_ns versions below don't do byte-swapping.
 * Neither do the standard versions now, these are just here
 * for older code.
 */
#define insw_ns(port, buf, ns)  _insw_ns((u16 *)((port)+_IO_BASE), (buf), (ns))
#define outsw_ns(port, buf, ns) _outsw_ns((u16 *)((port)+_IO_BASE), (buf), (ns))
#define insl_ns(port, buf, nl)  _insl_ns((u32 *)((port)+_IO_BASE), (buf), (nl))
#define outsl_ns(port, buf, nl) _outsl_ns((u32 *)((port)+_IO_BASE), (buf), (nl))


#define IO_SPACE_LIMIT ~0

/*
 * Enforce In-order Execution of I/O:
 * Acts as a barrier to ensure all previous I/O accesses have
 * completed before any further ones are issued.
 */
#define eieio() __asm__ __volatile__ ("eieio" : : : "memory");
#define sync()  __asm__ __volatile__ ("sync" : : : "memory");

/* Enforce in-order execution of data I/O.
 * No distinction between read/write on PPC; use eieio for all three.
 */
#define iobarrier_rw() eieio()
#define iobarrier_r()  eieio()
#define iobarrier_w()  eieio()

/*
 * Non ordered and non-swapping "raw" accessors
 */
static inline unsigned char __raw_readb(const volatile void __iomem *addr)
{
	return *(volatile unsigned char __force *)(addr);
}
static inline unsigned short __raw_readw(const volatile void __iomem *addr)
{
	return *(volatile unsigned short __force *)(addr);
}
static inline unsigned int __raw_readl(const volatile void __iomem *addr)
{
	return *(volatile unsigned int __force *)(addr);
}
static inline void __raw_writeb(unsigned char v, volatile void __iomem *addr)
{
	*(volatile unsigned char __force *)(addr) = v;
}
static inline void __raw_writew(unsigned short v, volatile void __iomem *addr)
{
	*(volatile unsigned short __force *)(addr) = v;
}
static inline void __raw_writel(unsigned int v, volatile void __iomem *addr)
{
	*(volatile unsigned int __force *)(addr) = v;
}

/*
 * 8, 16 and 32 bit, big and little endian I/O operations, with barrier.
 */
#define in_8 in_8
static inline u8 in_8(const volatile u8 __iomem *addr)
{
	u8 ret;

	__asm__ __volatile__("sync; lbz%U1%X1 %0,%1; twi 0,%0,0; isync"
			: "=r" (ret) : "m" (*addr));
	return ret;
}

#define out_8 out_8
static inline void out_8(volatile u8 __iomem *addr, u8 val)
{
	__asm__ __volatile__("sync;stb%U0%X0 %1,%0" : "=m" (*addr) : "r" (val));
}

#define in_le16 in_le16
static inline u16 in_le16(const volatile u16 __iomem *addr)
{
	u16 ret;

	__asm__ __volatile__("sync; lhbrx %0,0,%1; twi 0,%0,0; isync"
			: "=r" (ret) : "r" (addr), "m" (*addr));
	return ret;
}

#define in_be16 in_be16
static inline u16 in_be16(const volatile u16 __iomem *addr)
{
	u16 ret;

	__asm__ __volatile__("sync; lhz%U1%X1 %0,%1; twi 0,%0,0; isync"
			: "=r" (ret) : "m" (*addr));
	return ret;
}

#define out_le16 out_le16
static inline void out_le16(volatile u16 __iomem *addr, u16 val)
{
	__asm__ __volatile__("sync; sthbrx %1,0,%2"
			: "=m" (*addr) : "r" (val), "r" (addr));
}

#define out_be16 out_be16
static inline void out_be16(volatile u16 __iomem *addr, u16 val)
{
	__asm__ __volatile__("sync;sth%U0%X0 %1,%0" : "=m" (*addr) : "r" (val));
}

#define in_le32 in_le32
static inline u32 in_le32(const volatile u32 __iomem *addr)
{
	u32 ret;

	__asm__ __volatile__("sync; lwbrx %0,0,%1; twi 0,%0,0; isync"
			: "=r" (ret) : "r" (addr), "m" (*addr));
	return ret;
}

#define in_be32 in_be32
static inline u32 in_be32(const volatile u32 __iomem *addr)
{
	u32 ret;

	__asm__ __volatile__("sync; lwz%U1%X1 %0,%1; twi 0,%0,0; isync"
			: "=r" (ret) : "m" (*addr));
	return ret;
}

#define out_le32 out_le32
static inline void out_le32(volatile u32 __iomem *addr, u32 val)
{
	__asm__ __volatile__("sync; stwbrx %1,0,%2"
			: "=m" (*addr) : "r" (val), "r" (addr));
}

#define out_be32 out_be32
static inline void out_be32(volatile u32 __iomem *addr, u32 val)
{
	__asm__ __volatile__("sync;stw%U0%X0 %1,%0" : "=m" (*addr) : "r" (val));
}

/*
 * Clear and set bits in one shot. These macros can be used to clear and
 * set multiple bits in a register using a single call. These macros can
 * also be used to set a multiple-bit bit pattern using a mask, by
 * specifying the mask in the 'clear' parameter and the new bit pattern
 * in the 'set' parameter.
 */
#define clrbits(type, addr, clear) \
	out_##type((addr), in_##type(addr) & ~(clear))

#define setbits(type, addr, set) \
	out_##type((addr), in_##type(addr) | (set))

#define clrsetbits(type, addr, clear, set) \
	out_##type((addr), (in_##type(addr) & ~(clear)) | (set))

#define clrbits_be32(addr, clear) clrbits(be32, addr, clear)
#define setbits_be32(addr, set) setbits(be32, addr, set)
#define clrsetbits_be32(addr, clear, set) clrsetbits(be32, addr, clear, set)

/* these ones were originally in config.h */
unsigned char	in8(unsigned int);
void		out8(unsigned int, unsigned char);
unsigned short	in16(unsigned int);
unsigned short	in16r(unsigned int);
void		out16(unsigned int, unsigned short value);
void		out16r(unsigned int, unsigned short value);
unsigned long	in32(unsigned int);
unsigned long	in32r(unsigned int);
void		out32(unsigned int, unsigned long value);
void		out32r(unsigned int, unsigned long value);
void		ppcDcbf(unsigned long value);
void		ppcDcbi(unsigned long value);
void		ppcSync(void);
void		ppcDcbz(unsigned long value);

#include <asm-generic/io.h>

#endif
