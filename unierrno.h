/* $Id: errno.h,v 1.1.1.1 2003/07/02 13:56:58 eleph Exp $ */

#ifndef _I386_ERRNO_H
#define _I386_ERRNO_H

#define	UNIEPERM		 1	/* Operation not permitted */
#define	UNIENOENT		 2	/* No such file or directory */
#define	UNIESRCH		 3	/* No such process */
#define	UNIEINTR		 4	/* Interrupted system call */
#define	UNIEIO		 5	/* I/O error */
#define	UNIENXIO		 6	/* No such device or address */
#define	UNIE2BIG		 7	/* Arg list too long */
#define	UNIENOEXEC		 8	/* Exec format error */
#define	UNIEBADF		 9	/* Bad file number */
#define	UNIECHILD		10	/* No child processes */
#define	UNIEAGAIN		11	/* Try again */
#define	UNIENOMEM		12	/* Out of memory */
#define	UNIEACCES		13	/* Permission denied */
#define	UNIEFAULT		14	/* Bad address */
#define	UNIENOTBLK		15	/* Block device required */
#define	UNIEBUSY		16	/* Device or resource busy */
#define	UNIEEXIST		17	/* File exists */
#define	UNIEXDEV		18	/* Cross-device link */
#define	UNIENODEV		19	/* No such device */
#define	UNIENOTDIR		20	/* Not a directory */
#define	UNIEISDIR		21	/* Is a directory */
#define	UNIEINVAL		22	/* Invalid argument */
#define	UNIENFILE		23	/* File table overflow */
#define	UNIEMFILE		24	/* Too many open files */
#define	UNIENOTTY		25	/* Not a typewriter */
#define	UNIETXTBSY		26	/* Text file busy */
#define	UNIEFBIG		27	/* File too large */
#define	UNIENOSPC		28	/* No space left on device */
#define	UNIESPIPE		29	/* Illegal seek */
#define	UNIEROFS		30	/* Read-only file system */
#define	UNIEMLINK		31	/* Too many links */
#define	UNIEPIPE		32	/* Broken pipe */
#define	UNIEDOM		33	/* Math argument out of domain of func */
#define	UNIERANGE		34	/* Math result not representable */
#define	UNIEDEADLK		35	/* Resource deadlock would occur */
#define	UNIENAMETOOLONG	36	/* File name too long */
#define	UNIENOLCK		37	/* No record locks available */
#define	UNIENOSYS		38	/* Function not implemented */
#define	UNIENOTEMPTY	39	/* Directory not empty */
#define	UNIELOOP		40	/* Too many symbolic links encountered */
#define	UNIEWOULDBLOCK	EAGAIN	/* Operation would block */
#define	UNIENOMSG		42	/* No message of desired type */
#define	UNIEIDRM		43	/* Identifier removed */
#define	UNIECHRNG		44	/* Channel number out of range */
#define	UNIEL2NSYNC	45	/* Level 2 not synchronized */
#define	UNIEL3HLT		46	/* Level 3 halted */
#define	UNIEL3RST		47	/* Level 3 reset */
#define	UNIELNRNG		48	/* Link number out of range */
#define	UNIEUNATCH		49	/* Protocol driver not attached */
#define	UNIENOCSI		50	/* No CSI structure available */
#define	UNIEL2HLT		51	/* Level 2 halted */
#define	UNIEBADE		52	/* Invalid exchange */
#define	UNIEBADR		53	/* Invalid request descriptor */
#define	UNIEXFULL		54	/* Exchange full */
#define	UNIENOANO		55	/* No anode */
#define	UNIEBADRQC		56	/* Invalid request code */
#define	UNIEBADSLT		57	/* Invalid slot */

#define	UNIEDEADLOCK	UNIEDEADLK

#define	UNIEBFONT		59	/* Bad font file format */
#define	UNIENOSTR		60	/* Device not a stream */
#define	UNIENODATA		61	/* No data available */
#define	UNIETIME		62	/* Timer expired */
#define	UNIENOSR		63	/* Out of streams resources */
#define	UNIENONET		64	/* Machine is not on the network */
#define	UNIENOPKG		65	/* Package not installed */
#define	UNIEREMOTE		66	/* Object is remote */
#define	UNIENOLINK		67	/* Link has been severed */
#define	UNIEADV		68	/* Advertise error */
#define	UNIESRMNT		69	/* Srmount error */
#define	UNIECOMM		70	/* Communication error on send */
#define	UNIEPROTO		71	/* Protocol error */
#define	UNIEMULTIHOP	72	/* Multihop attempted */
#define	UNIEDOTDOT		73	/* RFS specific error */
#define	UNIEBADMSG		74	/* Not a data message */
#define	UNIEOVERFLOW	75	/* Value too large for defined data type */
#define	UNIENOTUNIQ	76	/* Name not unique on network */
#define	UNIEBADFD		77	/* File descriptor in bad state */
#define	UNIEREMCHG		78	/* Remote address changed */
#define	UNIELIBACC		79	/* Can not access a needed shared library */
#define	UNIELIBBAD		80	/* Accessing a corrupted shared library */
#define	UNIELIBSCN		81	/* .lib section in a.out corrupted */
#define	UNIELIBMAX		82	/* Attempting to link in too many shared libraries */
#define	UNIELIBEXEC	83	/* Cannot exec a shared library directly */
#define	UNIEILSEQ		84	/* Illegal byte sequence */
#define	UNIERESTART	85	/* Interrupted system call should be restarted */
#define	UNIESTRPIPE	86	/* Streams pipe error */
#define	UNIEUSERS		87	/* Too many users */
#define	UNIENOTSOCK	88	/* Socket operation on non-socket */
#define	UNIEDESTADDRREQ	89	/* Destination address required */
#define	UNIEMSGSIZE	90	/* Message too long */
#define	UNIEPROTOTYPE	91	/* Protocol wrong type for socket */
#define	UNIENOPROTOOPT	92	/* Protocol not available */
#define	UNIEPROTONOSUPPORT	93	/* Protocol not supported */
#define	UNIESOCKTNOSUPPORT	94	/* Socket type not supported */
#define	UNIEOPNOTSUPP	95	/* Operation not supported on transport endpoint */
#define	UNIEPFNOSUPPORT	96	/* Protocol family not supported */
#define	UNIEAFNOSUPPORT	97	/* Address family not supported by protocol */
#define	UNIEADDRINUSE	98	/* Address already in use */
#define	UNIEADDRNOTAVAIL	99	/* Cannot assign requested address */
#define	UNIENETDOWN	100	/* Network is down */
#define	UNIENETUNREACH	101	/* Network is unreachable */
#define	UNIENETRESET	102	/* Network dropped connection because of reset */
#define	UNIECONNABORTED	103	/* Software caused connection abort */
#define	UNIECONNRESET	104	/* Connection reset by peer */
#define	UNIENOBUFS		105	/* No buffer space available */
#define	UNIEISCONN		106	/* Transport endpoint is already connected */
#define	UNIENOTCONN	107	/* Transport endpoint is not connected */
#define	UNIESHUTDOWN	108	/* Cannot send after transport endpoint shutdown */
#define	UNIETOOMANYREFS	109	/* Too many references: cannot splice */
#define	UNIETIMEDOUT	110	/* Connection timed out */
#define	UNIECONNREFUSED	111	/* Connection refused */
#define	UNIEHOSTDOWN	112	/* Host is down */
#define	UNIEHOSTUNREACH	113	/* No route to host */
#define	UNIEALREADY	114	/* Operation already in progress */
#define	UNIEINPROGRESS	115	/* Operation now in progress */
#define	UNIESTALE		116	/* Stale NFS file handle */
#define	UNIEUCLEAN		117	/* Structure needs cleaning */
#define	UNIENOTNAM		118	/* Not a XENIX named type file */
#define	UNIENAVAIL		119	/* No XENIX semaphores available */
#define	UNIEISNAM		120	/* Is a named type file */
#define	UNIEREMOTEIO	121	/* Remote I/O error */
#define	UNIEDQUOT		122	/* Quota exceeded */

#define	UNIENOMEDIUM	123	/* No medium found */
#define	UNIEMEDIUMTYPE	124	/* Wrong medium type */

#endif
