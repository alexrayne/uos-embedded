#define JB_BX	0
#define JB_SI	1
#define JB_DI	2
#define JB_BP	3
#define JB_SP	4
#define JB_PC	5
#define JB_SIZE 24

		.section .text
longjmp:	.globl	longjmp

		movl	4(%esp), %ecx	/* User's jmp_buf in %ecx */

		movl	8(%esp), %eax	/* Second argument is return value */

		/* Save the return address now.  */
		movl	(JB_PC*4)(%ecx), %edx

		/* Restore registers.  */
		movl	(JB_BX*4)(%ecx), %ebx
		movl	(JB_SI*4)(%ecx), %esi
		movl	(JB_DI*4)(%ecx), %edi
		movl	(JB_BP*4)(%ecx), %ebp
		movl	(JB_SP*4)(%ecx), %esp

		/* Jump to saved PC.  */
		jmp	*%edx
