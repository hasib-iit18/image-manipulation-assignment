#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image.h"

/* BMP file headers must match the on-disk layout exactly, so we
   disable structure padding for these two structs. */
#pragma pack(push, 1)

typedef struct {
    unsigned short bfType;      /* must be 0x4D42 ('BM') */
    unsigned int   bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int   bfOffBits;   /* offset from file start to pixel data */
} BMPFileHeader;

typedef struct {
    unsigned int   biSize;
    int            biWidth;
    int            biHeight;    /* positive = bottom-up, negative = top-down */
    unsigned short biPlanes;
    unsigned short biBitCount;  /* we only support 24 */
    unsigned int   biCompression; /* we only support 0 = BI_RGB */
    unsigned int   biSizeImage;
    int            biXPelsPerMeter;
    int            biYPelsPerMeter;
    unsigned int   biClrUsed;
    unsigned int   biClrImportant;
} BMPInfoHeader;

#pragma pack(pop)

Image* image_create(int width, int height)
{
    if (width <= 0 || height <= 0) return NULL;

    Image *img = (Image*) malloc(sizeof(Image));
    if (!img) return NULL;

    img->width  = width;
    img->height = height;
    img->data   = (Pixel*) malloc(sizeof(Pixel) * (size_t)width * (size_t)height);

    if (!img->data) {
        free(img);
        return NULL;
    }
    return img;
}

void image_free(Image *img)
{
    if (!img) return;
    if (img->data) free(img->data);
    free(img);
}

Image* image_copy(const Image *img)
{
    if (!img) return NULL;

    Image *copy = image_create(img->width, img->height);
    if (!copy) return NULL;

    memcpy(copy->data, img->data, sizeof(Pixel) * (size_t)img->width * (size_t)img->height);
    return copy;
}

Image* image_load_bmp(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    BMPFileHeader fh;
    BMPInfoHeader ih;

    if (fread(&fh, sizeof(fh), 1, fp) != 1) { fclose(fp); return NULL; }
    if (fh.bfType != 0x4D42) { fclose(fp); return NULL; } /* not "BM" */

    if (fread(&ih, sizeof(ih), 1, fp) != 1) { fclose(fp); return NULL; }

    /* This assignment only supports 24-bit uncompressed BMP */
    if (ih.biBitCount != 24 || ih.biCompression != 0) {
        fclose(fp);
        return NULL;
    }

    int width  = ih.biWidth;
    int height = ih.biHeight;
    int top_down = 0;

    if (height < 0) {
        top_down = 1;
        height = -height;
    }

    Image *img = image_create(width, height);
    if (!img) { fclose(fp); return NULL; }

    if (fseek(fp, (long)fh.bfOffBits, SEEK_SET) != 0) {
        image_free(img);
        fclose(fp);
        return NULL;
    }

    /* Each BMP row is padded to a multiple of 4 bytes */
    int row_padded = (width * 3 + 3) & (~3);
    unsigned char *row_buf = (unsigned char*) malloc((size_t)row_padded);
    if (!row_buf) { image_free(img); fclose(fp); return NULL; }

    for (int y = 0; y < height; y++) {
        if (fread(row_buf, 1, (size_t)row_padded, fp) != (size_t)row_padded) {
            free(row_buf);
            image_free(img);
            fclose(fp);
            return NULL;
        }

        /* BMP is normally stored bottom-up; we store our image top-down
           (row 0 = top row), so we flip while reading unless the file
           already declared itself top-down. */
        int dest_y = top_down ? y : (height - 1 - y);

        for (int x = 0; x < width; x++) {
            unsigned char b = row_buf[x * 3 + 0];
            unsigned char g = row_buf[x * 3 + 1];
            unsigned char r = row_buf[x * 3 + 2];

            Pixel p;
            p.r = r; p.g = g; p.b = b;
            img->data[dest_y * width + x] = p;
        }
    }

    free(row_buf);
    fclose(fp);
    return img;
}

int image_save_bmp(const Image *img, const char *filename)
{
    if (!img) return 0;

    FILE *fp = fopen(filename, "wb");
    if (!fp) return 0;

    int width  = img->width;
    int height = img->height;
    int row_padded = (width * 3 + 3) & (~3);
    unsigned int data_size = (unsigned int)row_padded * (unsigned int)height;

    BMPFileHeader fh;
    BMPInfoHeader ih;
    memset(&fh, 0, sizeof(fh));
    memset(&ih, 0, sizeof(ih));

    fh.bfType    = 0x4D42; /* "BM" */
    fh.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
    fh.bfSize    = fh.bfOffBits + data_size;

    ih.biSize          = sizeof(BMPInfoHeader);
    ih.biWidth         = width;
    ih.biHeight        = height; /* positive => bottom-up, standard BMP */
    ih.biPlanes        = 1;
    ih.biBitCount      = 24;
    ih.biCompression   = 0;
    ih.biSizeImage     = data_size;
    ih.biXPelsPerMeter = 2835; /* ~72 DPI, not important for this assignment */
    ih.biYPelsPerMeter = 2835;

    fwrite(&fh, sizeof(fh), 1, fp);
    fwrite(&ih, sizeof(ih), 1, fp);

    unsigned char *row_buf = (unsigned char*) calloc(1, (size_t)row_padded);
    if (!row_buf) { fclose(fp); return 0; }

    /* Write bottom-up, since our image is stored top-down internally */
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            Pixel p = img->data[y * width + x];
            row_buf[x * 3 + 0] = p.b;
            row_buf[x * 3 + 1] = p.g;
            row_buf[x * 3 + 2] = p.r;
        }
        fwrite(row_buf, 1, (size_t)row_padded, fp);
    }

    free(row_buf);
    fclose(fp);
    return 1;
}
