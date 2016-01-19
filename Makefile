EXEC       = GenPCE
DEPDIR     = mtl utils core
MROOT      = $(PWD)/minisat
CFLAGS     = -Wall -Wno-parentheses
include $(MROOT)/mtl/template.mk
