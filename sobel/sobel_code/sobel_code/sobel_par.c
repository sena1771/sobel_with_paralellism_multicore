/* Example sobel code*/
/* By Vince Weaver <vincent.weaver@maine.edu> */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>

#include <jpeglib.h>
#include <pthread.h> /*pthread libray for multithreading */


/* Filters */
static int sobel_x_filter[3][3]={{-1,0,+1},{-2,0,+2},{-1,0,+1}};
static int sobel_y_filter[3][3]={{-1,-2,-1},{0,0,0},{1,2,+1}};

/* Structure describing the image */
struct image_t {
	int x;
	int y;
	int depth;	/* bytes */
	unsigned char *pixels;
};

/* Structure for passing data to thread function */
/* This structure holds all the information needed by each thread */
struct thread_data_t {
    struct image_t *old;
    struct image_t *new;
    int (*filter)[3][3];
    int start_row;
    int end_row;
    int depth;
    int width;
};


/* New thread function for convolution */
/* Each thread runs this function to perform convolution over its assigned rows */
void *thread_convolve(void *arg) {
    struct thread_data_t *data = (struct thread_data_t *)arg;
    int x, y, k, l, d;
    uint32_t color;
    int sum;

    /* Perform convolution over assigned rows */
    for (d = 0; d < data->depth; d++) {
        for (y = data->start_row; y < data->end_row; y++) {
            for (x = 1; x < data->old->x - 1; x++) {
                sum = 0;
                for (k = -1; k <= 1; k++) {
                    for (l = -1; l <= 1; l++) {
                        color = data->old->pixels[((y + l) * data->width) + (x * data->depth + d + k * data->depth)];
                        sum += color * (*data->filter)[k + 1][l + 1];
                    }
                }

                if (sum < 0)
                    sum = 0;
                if (sum > 255)
                    sum = 255;

                data->new->pixels[(y * data->width) + x * data->depth + d] = sum;
            }
        }
    }

    return NULL;
}

static int load_jpeg(char *filename, struct image_t *image) {

	FILE *fff;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW output_data;
	unsigned int scanline_len;
	int scanline_count=0;

	fff=fopen(filename,"rb");
	if (fff==NULL) {
		fprintf(stderr, "Could not load %s: %s\n",
			filename, strerror(errno));
		return -1;
	}

	/* set up jpeg error routines */
	cinfo.err = jpeg_std_error(&jerr);

	/* Initialize cinfo */
	jpeg_create_decompress(&cinfo);

	/* Set input file */
	jpeg_stdio_src(&cinfo, fff);

	/* read header */
	jpeg_read_header(&cinfo, TRUE);

	/* Start decompressor */
	jpeg_start_decompress(&cinfo);

	printf("output_width=%d, output_height=%d, output_components=%d\n",
		cinfo.output_width,
		cinfo.output_height,
		cinfo.output_components);

	image->x=cinfo.output_width;
	image->y=cinfo.output_height;
	image->depth=cinfo.output_components;

	scanline_len = cinfo.output_width * cinfo.output_components;
	image->pixels=malloc(cinfo.output_width * cinfo.output_height * cinfo.output_components);

	while (scanline_count < cinfo.output_height) {
		output_data = (image->pixels + (scanline_count * scanline_len));
		jpeg_read_scanlines(&cinfo, &output_data, 1);
		scanline_count++;
	}

	/* Finish decompressing */
	jpeg_finish_decompress(&cinfo);

	jpeg_destroy_decompress(&cinfo);

	fclose(fff);

	return 0;
}

static int store_jpeg(char *filename, struct image_t *image) {

	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	int quality=90; /* % */
	int i;

	FILE *fff;

	JSAMPROW row_pointer[1];
	int row_stride;

	/* setup error handler */
	cinfo.err = jpeg_std_error(&jerr);

	/* initialize jpeg compression object */
	jpeg_create_compress(&cinfo);

	/* Open file */
	fff = fopen(filename, "wb");
	if (fff==NULL) {
		fprintf(stderr, "can't open %s: %s\n",
			filename,strerror(errno));
		return -1;
	}

	jpeg_stdio_dest(&cinfo, fff);

	/* Set compression parameters */
	cinfo.image_width = image->x;
	cinfo.image_height = image->y;
	cinfo.input_components = image->depth;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);

	/* start compressing */
	jpeg_start_compress(&cinfo, TRUE);

	row_stride=image->x*image->depth;

	for(i=0;i<image->y;i++) {
		row_pointer[0] = & image->pixels[i * row_stride];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	/* finish compressing */
	jpeg_finish_compress(&cinfo);

	/* close file */
	fclose(fff);

	/* clean up */
	jpeg_destroy_compress(&cinfo);

	return 0;
}

static int combine(struct image_t *s_x,
			struct image_t *s_y,
			struct image_t *new) {
	int i;
	int out;

	for(i=0;i<( s_x->depth * s_x->x * s_x->y );i++) {

		out=sqrt(
			(s_x->pixels[i]*s_x->pixels[i])+
			(s_y->pixels[i]*s_y->pixels[i])
			);
		if (out>255) out=255;
		if (out<0) out=0;
		new->pixels[i]=out;
	}

	return 0;
}

int main(int argc, char **argv) {

	struct image_t image,sobel_x,sobel_y,new_image;
	int num_threads = 1; /* Default number of threads */

	/* Check command line usage */
    	if (argc < 2) {
        	fprintf(stderr, "Usage: %s image_file [num_threads]\n", argv[0]);
        	return -1;
    	}

	/* Getting number of threads from command line */
    	if (argc >= 3) {
        	num_threads = atoi(argv[2]);
        	if (num_threads <= 0) {
            		fprintf(stderr, "Invalid number of threads specified.\n");
            		return -1;
        	}
    	}

	/* Load an image */
	load_jpeg(argv[1],&image);

	/* Allocate space for output image */
	new_image.x=image.x;
	new_image.y=image.y;
	new_image.depth=image.depth;
	new_image.pixels=malloc(image.x*image.y*image.depth*sizeof(char));

	/* Allocate space for output image */
	sobel_x.x=image.x;
	sobel_x.y=image.y;
	sobel_x.depth=image.depth;
	sobel_x.pixels=malloc(image.x*image.y*image.depth*sizeof(char));

	/* Allocate space for output image */
	sobel_y.x=image.x;
	sobel_y.y=image.y;
	sobel_y.depth=image.depth;
	sobel_y.pixels=malloc(image.x*image.y*image.depth*sizeof(char));


    /* Allocating memory for threads and thread data structures */
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    struct thread_data_t *thread_data = malloc(num_threads * sizeof(struct thread_data_t));

    /* Calculating image dimensions */
    int i;
    int width = image.x * image.depth;            /* Total number of bytes in a row */
    int depth = image.depth;                      /* Number of color channels (e.g., 3 for RGB) */
    int total_rows = image.y - 2;                 /* Number of rows to process (excluding borders) */
    int rows_per_thread = total_rows / num_threads; /* Number of rows each thread will process */
    int remaining_rows = total_rows % num_threads; /* Extra rows to distribute among threads */
    int current_row = 1;                          /* Start from 1 to exclude border */

    /* First convolution with sobel_x_filter */
    /* Divide the work among threads */
    for (i = 0; i < num_threads; i++) {
        /* Initialize thread data */
        thread_data[i].old = &image;
        thread_data[i].new = &sobel_x;
        thread_data[i].filter = &sobel_x_filter;
        thread_data[i].depth = depth;
        thread_data[i].width = width;
        thread_data[i].start_row = current_row;
        thread_data[i].end_row = current_row + rows_per_thread;

        /* Distribute any remaining rows */
        if (remaining_rows > 0) {
            thread_data[i].end_row++;
            remaining_rows--;
        }
        current_row = thread_data[i].end_row;

        /* Create thread */
        pthread_create(&threads[i], NULL, thread_convolve, (void *)&thread_data[i]);
    }

    /* Wait for threads to finish */
    for (i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Reset remaining_rows and current_row for second convolution */
    remaining_rows = total_rows % num_threads;
    current_row = 1;

    /* Second convolution with sobel_y_filter */
    /* Dividing the work among threads */
    for (i = 0; i < num_threads; i++) {
        /* Initialize thread data */
        thread_data[i].old = &image;
        thread_data[i].new = &sobel_y;
        thread_data[i].filter = &sobel_y_filter;
        thread_data[i].depth = depth;
        thread_data[i].width = width;
        thread_data[i].start_row = current_row;
        thread_data[i].end_row = current_row + rows_per_thread;

        /* Distribute any remaining rows */
        if (remaining_rows > 0) {
            thread_data[i].end_row++;
            remaining_rows--;
        }
        current_row = thread_data[i].end_row;

        /* Create thread */
        pthread_create(&threads[i], NULL, thread_convolve, (void *)&thread_data[i]);
    }

    /* Wait for threads to finish */
    for (i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

	/* Combine to form output */
	combine(&sobel_x,&sobel_y,&new_image);

	/* Write data back out to disk */
	store_jpeg("out.jpg",&new_image);

	/* Free allocated memory */
    	free(image.pixels);
    	free(sobel_x.pixels);
    	free(sobel_y.pixels);
    	free(new_image.pixels);
    	free(threads);
    	free(thread_data);

	return 0;
}
