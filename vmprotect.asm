.code

__vm_protect proc
	mov r10,rcx
	mov eax,50h
	syscall
	ret
__vm_protect endp

end