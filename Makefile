all:
	gcc fcd_fm.c -Wall -Wextra -lasound -lm -o fcd_fm
	gcc fcd_ssb.c -Wall -Wextra -lasound -lm -o fcd_ssb
