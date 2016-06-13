/**
 *
 *  Copyright 2016 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "common/alloc.h"
#include "common/file_io.h"
#include "iqa/ssim_tools.h"

int ssim(const char *ref_path, const char *dis_path, int w, int h, const char *fmt)
{
	double score = 0;
	double l_score = 0, c_score = 0, s_score = 0;
	number_t *ref_buf = 0;
	number_t *dis_buf = 0;
	number_t *temp_buf = 0;

	FILE *ref_rfile = 0;
	FILE *dis_rfile = 0;
	size_t data_sz;
	int stride;
	int ret = 1;

	if (w <= 0 || h <= 0 || (size_t)w > ALIGN_FLOOR(INT_MAX) / sizeof(number_t))
	{
		goto fail_or_end;
	}

	stride = ALIGN_CEIL(w * sizeof(number_t));

	if ((size_t)h > SIZE_MAX / stride)
	{
		goto fail_or_end;
	}

	data_sz = (size_t)stride * h;

	if (!(ref_buf = aligned_malloc(data_sz, MAX_ALIGN)))
	{
		printf("error: aligned_malloc failed for ref_buf.\n");
		fflush(stdout);
		goto fail_or_end;
	}
	if (!(dis_buf = aligned_malloc(data_sz, MAX_ALIGN)))
	{
		printf("error: aligned_malloc failed for dis_buf.\n");
		fflush(stdout);
		goto fail_or_end;
	}
	if (!(temp_buf = aligned_malloc(data_sz * 2, MAX_ALIGN)))
	{
		printf("error: aligned_malloc failed for temp_buf.\n");
		fflush(stdout);
		goto fail_or_end;
	}

	if (!(ref_rfile = fopen(ref_path, "rb")))
	{
		printf("error: fopen ref_path %s failed\n", ref_path);
		fflush(stdout);
		goto fail_or_end;
	}
	if (!(dis_rfile = fopen(dis_path, "rb")))
	{
		printf("error: fopen dis_path %s failed.\n", dis_path);
		fflush(stdout);
		goto fail_or_end;
	}

	size_t offset;
	if (!strcmp(fmt, "yuv420p") || !strcmp(fmt, "yuv420p10le"))
	{
		if ((w * h) % 2 != 0)
		{
			printf("error: (w * h) %% 2 != 0, w = %d, h = %d.\n", w, h);
			fflush(stdout);
			goto fail_or_end;
		}
		offset = w * h / 2;
	}
	else if (!strcmp(fmt, "yuv422p") || !strcmp(fmt, "yuv422p10le"))
	{
		offset = w * h;
	}
	else if (!strcmp(fmt, "yuv444p") || !strcmp(fmt, "yuv444p10le"))
	{
		offset = w * h * 2;
	}
	else
	{
		printf("error: unknown format %s.\n", fmt);
		fflush(stdout);
		goto fail_or_end;
	}

	int frm_idx = 0;
	while (1)
	{
		// read ref y
		if (!strcmp(fmt, "yuv420p") || !strcmp(fmt, "yuv422p") || !strcmp(fmt, "yuv444p"))
		{
			ret = read_image_b(ref_rfile, ref_buf, 0, w, h, stride);
		}
		else if (!strcmp(fmt, "yuv420p10le") || !strcmp(fmt, "yuv422p10le") || !strcmp(fmt, "yuv444p10le"))
		{
			ret = read_image_w(ref_rfile, ref_buf, 0, w, h, stride);
		}
		else
		{
			printf("error: unknown format %s.\n", fmt);
			fflush(stdout);
			goto fail_or_end;
		}
		if (ret)
		{
			if (feof(ref_rfile))
			{
				ret = 0; // OK if end of file
			}
			goto fail_or_end;
		}

		// read dis y
		if (!strcmp(fmt, "yuv420p") || !strcmp(fmt, "yuv422p") || !strcmp(fmt, "yuv444p"))
		{
			ret = read_image_b(dis_rfile, dis_buf, 0, w, h, stride);
		}
		else if (!strcmp(fmt, "yuv420p10le") || !strcmp(fmt, "yuv422p10le") || !strcmp(fmt, "yuv444p10le"))
		{
			ret = read_image_w(dis_rfile, dis_buf, 0, w, h, stride);
		}
		else
		{
			printf("error: unknown format %s.\n", fmt);
			fflush(stdout);
			goto fail_or_end;
		}
		if (ret)
		{
			if (feof(dis_rfile))
			{
				ret = 0; // OK if end of file
			}
			goto fail_or_end;
		}

		// compute
		ret = compute_ssim(ref_buf, dis_buf, w, h, stride, stride, &score, &l_score, &c_score, &s_score);
		if (ret)
		{
			printf("error: compute_ssim failed.\n");
			fflush(stdout);
			goto fail_or_end;
		}

		// print
		printf("ssim: %d %f\n", frm_idx, score);
		printf("ssim_l: %d %f\n", frm_idx, l_score);
		printf("ssim_c: %d %f\n", frm_idx, c_score);
		printf("ssim_s: %d %f\n", frm_idx, s_score);
		fflush(stdout);

		// ref skip u and v
		if (!strcmp(fmt, "yuv420p") || !strcmp(fmt, "yuv422p") || !strcmp(fmt, "yuv444p"))
		{
			if (fread(temp_buf, 1, offset, ref_rfile) != (size_t)offset)
			{
				printf("error: ref fread u and v failed.\n");
				fflush(stdout);
				goto fail_or_end;
			}
		}
		else if (!strcmp(fmt, "yuv420p10le") || !strcmp(fmt, "yuv422p10le") || !strcmp(fmt, "yuv444p10le"))
		{
			if (fread(temp_buf, 2, offset, ref_rfile) != (size_t)offset)
			{
				printf("error: ref fread u and v failed.\n");
				fflush(stdout);
				goto fail_or_end;
			}
		}
		else
		{
			printf("error: unknown format %s.\n", fmt);
			fflush(stdout);
			goto fail_or_end;
		}

		// dis skip u and v
		if (!strcmp(fmt, "yuv420p") || !strcmp(fmt, "yuv422p") || !strcmp(fmt, "yuv444p"))
		{
			if (fread(temp_buf, 1, offset, dis_rfile) != (size_t)offset)
			{
				printf("error: dis fread u and v failed.\n");
				fflush(stdout);
				goto fail_or_end;
			}
		}
		else if (!strcmp(fmt, "yuv420p10le") || !strcmp(fmt, "yuv422p10le") || !strcmp(fmt, "yuv444p10le"))
		{
			if (fread(temp_buf, 2, offset, dis_rfile) != (size_t)offset)
			{
				printf("error: dis fread u and v failed.\n");
				fflush(stdout);
				goto fail_or_end;
			}
		}
		else
		{
			printf("error: unknown format %s.\n", fmt);
			fflush(stdout);
			goto fail_or_end;
		}

		frm_idx++;
	}

	ret = 0;

fail_or_end:
	if (ref_rfile)
	{
		fclose(ref_rfile);
	}
	if (dis_rfile)
	{
		fclose(dis_rfile);
	}
	aligned_free(ref_buf);
	aligned_free(dis_buf);
	aligned_free(temp_buf);

	return ret;
}


static void usage(void)
{
	puts("usage: ssim fmt ref dis w h\n"
		 "fmts:\n"
		 "\tyuv420p\n"
		 "\tyuv422p\n"
		 "\tyuv444p\n"
		 "\tyuv420p10le\n"
		 "\tyuv422p10le\n"
		 "\tyuv444p10le"
	);
}

int main(int argc, const char **argv)
{
	const char *ref_path;
	const char *dis_path;
	const char *fmt;
	int w;
	int h;
	int ret;

	if (argc < 6) {
		usage();
		return 2;
	}

	fmt		 = argv[1];
	ref_path = argv[2];
	dis_path = argv[3];
	w        = atoi(argv[4]);
	h        = atoi(argv[5]);

	if (w <= 0 || h <= 0) {
		usage();
		return 2;
	}

	ret = ssim(ref_path, dis_path, w, h, fmt);

	if (ret)
		return ret;

	return 0;
}
