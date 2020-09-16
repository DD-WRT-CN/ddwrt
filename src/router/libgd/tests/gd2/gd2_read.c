#include "gd.h"
#include <stdio.h>
#include <stdlib.h>
#include "gdtest.h"

int main(int argc, char *argv[])
{
	int error, i = 0;
	gdImagePtr im, exp;
	FILE *fp;
	char *path[] = {
			"conv_test.gd2",
			"invalid_neg_size.gd2",
			"invalid_header.gd2",
			NULL
	};
	char *path_exp[] = {
			"conv_test_exp.png",
			NULL,
			NULL,
			NULL
	};
	while (path[i] != NULL) {
		fp = gdTestFileOpen2("gd2", path[i]);
		if (!fp) {
			gdTestErrorMsg("failed, cannot open file: %s\n", path[0]);
			return 1;
		}
		im = gdImageCreateFromGd2(fp);
		fclose(fp);

		if (path_exp[i] != NULL) {
			fp = gdTestFileOpen2("gd2", path_exp[i]);
			if (!fp) {
				gdTestErrorMsg("failed, cannot open file: %s\n", path_exp[i]);
				return 1;
			}
			exp = gdImageCreateFromPng(fp);
			if (!gdAssertImageEquals(exp, im)) {
				gdTestErrorMsg("image %s differs from expected result\n", path[i]);
				gdImageDestroy(im);
				error = 1;
			} else {
				error = 0;
			}
			if (exp) {
				gdImageDestroy(exp);
			}
		} else {
			/* expected to fail */
			if (im) {
				gdTestErrorMsg("image %s should have failed to be loaded\n", path[i]);
				gdImageDestroy(im);
				error = 1;
			} else {
				error = 0;
			}
		}
		i++;
	}
	return error;
}
