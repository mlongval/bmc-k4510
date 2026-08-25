
/*
	PUT CHAR -- to JIM, the terminal. Mad Pascal's EOL is $0D; JIM is a
	VT100, so a CR gets its LF here.
*/

.proc	@putchar (.byte a) .reg

	sta k4_term
	cmp #13
	bne done
	lda #10
	sta k4_term
done	rts
.endp
