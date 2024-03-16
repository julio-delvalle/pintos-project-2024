include ./Makefile

SUFFIXES = %_TESTS %_EXTRA_GRADES

pintos-vscode-discover:
    $(info BEGIN_TESTS)
    $(foreach v,\
        $(filter $(SUFFIXES), $(.VARIABLES)),\
        $(info $($(v)))\
    )
    $(info END_TESTS)
