# $NetBSD: cond-func-defined.mk,v 1.10 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the defined() function in .if conditions.

DEF=		defined
${:UA B}=	variable name with spaces

.if !defined(DEF)
.  error
.endif

# Horizontal whitespace (space tab) after the opening parenthesis is ignored.
.if !defined( 	DEF)
.  error
.endif

# Horizontal whitespace (space tab) before the closing parenthesis is ignored.
.if !defined(DEF 	)
.  error
.endif

# The argument of a function must not directly contain whitespace.
# expect+1: Missing closing parenthesis for defined()
.if !defined(A B)
.  error
.endif

# If necessary, the whitespace can be generated by a variable expression.
.if !defined(${:UA B})
.  error
.endif

# expect+1: Missing closing parenthesis for defined()
.if defined(DEF
.  error
.else
.  error
.endif

# Variables from .for loops are not defined.
# See directive-for.mk for more details.
.for var in value
.  if defined(var)
.    error
.  else
# expect+1: In .for loops, variable expressions for the loop variables are
.    info In .for loops, variable expressions for the loop variables are
# expect+1: substituted at evaluation time.  There is no actual variable
.    info substituted at evaluation time.  There is no actual variable
# expect+1: involved, even if it feels like it.
.    info involved, even if it feels like it.
.  endif
.endfor

# Neither of the conditions is true.  Before July 2020, the right-hand
# condition was evaluated even though it was irrelevant.
.if defined(UNDEF) && ${UNDEF:Mx} != ""
.  error
.endif

all: .PHONY
