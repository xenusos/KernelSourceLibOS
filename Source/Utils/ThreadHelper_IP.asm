;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;    Purpose:                                                         ;;
;;    Author: Reece W.                                                 ;;
;;    License: All Rights Reserved J. Reece Wilson (See License.txt)   ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

PUBLIC ?GetIP@ThreadHelpers@@YAPEAXXZ

.code

?GetIP@ThreadHelpers@@YAPEAXXZ PROC
    lea     rax, [rsp+8]
    mov     [rsp+8], rcx
    mov     rax, [rax-8]
    ret 
?GetIP@ThreadHelpers@@YAPEAXXZ ENDP

END
