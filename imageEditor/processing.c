#include <stdlib.h>
#include "processing.h"

/* Clamp an integer value into [lo, hi] */
static int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void apply_grayscale(Image *img)
{
    if (!img) return;
    int n = img->width * img->height;

    for (int i = 0; i < n; i++) {
        Pixel p = img->data[i];
        double gray = 0.299 * p.r + 0.587 * p.g + 0.114 * p.b;
        unsigned char g = (unsigned char)(gray + 0.5); /* round to nearest */
        img->data[i].r = g;
        img->data[i].g = g;
        img->data[i].b = g;
    }
}

void apply_brightness(Image *img, int delta)
{
    if (!img) return;
    int n = img->width * img->height;

    for (int i = 0; i < n; i++) {
        int r = (int)img->data[i].r + delta;
        int g = (int)img->data[i].g + delta;
        int b = (int)img->data[i].b + delta;

        img->data[i].r = (unsigned char)clampi(r, 0, 255);
        img->data[i].g = (unsigned char)clampi(g, 0, 255);
        img->data[i].b = (unsigned char)clampi(b, 0, 255);
    }
}

void apply_invert(Image *img)
{
    if (!img) return;
    int n = img->width * img->height;

    for (int i = 0; i < n; i++) {
        img->data[i].r = 255 - img->data[i].r;
        img->data[i].g = 255 - img->data[i].g;
        img->data[i].b = 255 - img->data[i].b;
    }
}

void apply_flip_horizontal(Image *img)
{
    if (!img) return;
    int w = img->width, h = img->height;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w / 2; x++) {
            Pixel tmp = img->data[y * w + x];
            img->data[y * w + x]         = img->data[y * w + (w - 1 - x)];
            img->data[y * w + (w - 1 - x)] = tmp;
        }
    }
}

void apply_flip_vertical(Image *img)
{
    if (!img) return;
    int w = img->width, h = img->height;

    for (int y = 0; y < h / 2; y++) {
        for (int x = 0; x < w; x++) {
            Pixel tmp = img->data[y * w + x];
            img->data[y * w + x]             = img->data[(h - 1 - y) * w + x];
            img->data[(h - 1 - y) * w + x]   = tmp;
        }
    }
}

Image* apply_rotate90(const Image *img)
{
    if (!img) return NULL;

    int old_w = img->width;
    int old_h = img->height;
    int new_w = old_h;
    int new_h = old_w;

    Image *out = image_create(new_w, new_h);
    if (!out) return NULL;

    for (int y = 0; y < old_h; y++) {
        for (int x = 0; x < old_w; x++) {
            Pixel p = img->data[y * old_w + x];

            /* Clockwise rotation mapping */
            int nx = old_h - 1 - y;
            int ny = x;

            out->data[ny * new_w + nx] = p;
        }
    }
    return out;
}

Image* apply_crop(const Image *img, int x, int y, int w, int h)
{
    if (!img) return NULL;
    if (x < 0 || y < 0 || w <= 0 || h <= 0) return NULL;
    if (x + w > img->width || y + h > img->height) return NULL;

    Image *out = image_create(w, h);
    if (!out) return NULL;

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            out->data[j * w + i] = img->data[(y + j) * img->width + (x + i)];
        }
    }
    return out;
}

Image* apply_blur(const Image *img)
{
    if (!img) return NULL;
    int w = img->width, h = img->height;

    Image *out = image_create(w, h);
    if (!out) return NULL;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sum_r = 0, sum_g = 0, sum_b = 0, count = 0;

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = clampi(x + dx, 0, w - 1);
                    int ny = clampi(y + dy, 0, h - 1);
                    Pixel p = img->data[ny * w + nx];

                    sum_r += p.r;
                    sum_g += p.g;
                    sum_b += p.b;
                    count++;
                }
            }

            Pixel out_p;
            out_p.r = (unsigned char)(sum_r / count);
            out_p.g = (unsigned char)(sum_g / count);
            out_p.b = (unsigned char)(sum_b / count);
            out->data[y * w + x] = out_p;
        }
    }
    return out;
}

Image* apply_sharpen(const Image *img)
{
    if (!img) return NULL;
    int w = img->width, h = img->height;

    Image *out = image_create(w, h);
    if (!out) return NULL;

    static const int kernel[3][3] = {
        { 0, -1,  0 },
        { -1, 5, -1 },
        { 0, -1,  0 }
    };

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sum_r = 0, sum_g = 0, sum_b = 0;

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = clampi(x + dx, 0, w - 1);
                    int ny = clampi(y + dy, 0, h - 1);
                    Pixel p = img->data[ny * w + nx];
                    int k = kernel[dy + 1][dx + 1];

                    sum_r += p.r * k;
                    sum_g += p.g * k;
                    sum_b += p.b * k;
                }
            }

            Pixel out_p;
            out_p.r = (unsigned char)clampi(sum_r, 0, 255);
            out_p.g = (unsigned char)clampi(sum_g, 0, 255);
            out_p.b = (unsigned char)clampi(sum_b, 0, 255);
            out->data[y * w + x] = out_p;
        }
    }
    return out;
}
