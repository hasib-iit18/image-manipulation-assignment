#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iup.h>

#include "image.h"
#include "processing.h"

/* A single entry in the undo history: a saved copy of the image,
   plus a pointer to the entry before it (further back in history). */
typedef struct UndoNode {
    Image *img;
    struct UndoNode *next;
} UndoNode;

/* ---- Application state ---- */
static Image    *g_image        = NULL; /* currently loaded/edited image */
static UndoNode *g_undo_stack   = NULL; /* full undo history (top = most recent) */
static Ihandle  *g_canvas_label = NULL; /* label used to display the image */
static Ihandle  *g_iup_image    = NULL; /* the IUP image handle currently shown */
static Ihandle  *g_dlg          = NULL;

/* Push a copy of the current image onto the undo history.
   Called BEFORE any operation that changes g_image, so we can always
   step back through every previous edit, one at a time. */
static void save_undo(void)
{
    if (!g_image) return;

    UndoNode *node = (UndoNode*) malloc(sizeof(UndoNode));
    if (!node) return;

    node->img  = image_copy(g_image);
    node->next = g_undo_stack;
    g_undo_stack = node;
}

/* Free the entire undo history (used when opening a new image). */
static void clear_undo_stack(void)
{
    while (g_undo_stack) {
        UndoNode *next = g_undo_stack->next;
        image_free(g_undo_stack->img);
        free(g_undo_stack);
        g_undo_stack = next;
    }
}

/* Redraw the on-screen image from the current g_image contents. */
static void refresh_display(void)
{
    if (!g_image) return;

    int w = g_image->width;
    int h = g_image->height;

    unsigned char *buf = (unsigned char*) malloc((size_t)w * (size_t)h * 3);
    if (!buf) return;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            Pixel p = g_image->data[y * w + x];
            int idx = (y * w + x) * 3;
            buf[idx + 0] = p.r;
            buf[idx + 1] = p.g;
            buf[idx + 2] = p.b;
        }
    }

    Ihandle *new_img = IupImageRGB(w, h, buf);
    free(buf);

    if (!new_img) {
        IupMessage("Error", "Failed to create the display image.");
        return;
    }

    if (g_iup_image) {
        IupDestroy(g_iup_image);
    }
    g_iup_image = new_img;

    IupSetAttributeHandle(g_canvas_label, "IMAGE", g_iup_image);
    IupRedraw(g_canvas_label, 0);
}

static int no_image_loaded(void)
{
    if (!g_image) {
        IupMessage("Error", "No image has been loaded yet.\nUse Open first.");
        return 1;
    }
    return 0;
}

/* ---------------- Callbacks ---------------- */

static int cb_open(Ihandle *self)
{
    (void)self;
    Ihandle *filedlg = IupFileDlg();
    IupSetAttribute(filedlg, "DIALOGTYPE", "OPEN");
    IupSetAttribute(filedlg, "EXTFILTER", "BMP Files|*.bmp|All Files|*.*|");
    IupSetAttribute(filedlg, "TITLE", "Open BMP Image");

    IupPopup(filedlg, IUP_CENTER, IUP_CENTER);

    if (IupGetInt(filedlg, "STATUS") != -1) {
        char *filename = IupGetAttribute(filedlg, "VALUE");
        Image *img = image_load_bmp(filename);

        if (!img) {
            IupMessage("Error", "Could not open this file.\nOnly 24-bit uncompressed BMP images are supported.");
        } else {
            if (g_image) image_free(g_image);
            clear_undo_stack();
            g_image = img;
            refresh_display();
        }
    }

    IupDestroy(filedlg);
    return IUP_DEFAULT;
}

static int cb_save(Ihandle *self)
{
    (void)self;
    if (no_image_loaded()) return IUP_DEFAULT;

    Ihandle *filedlg = IupFileDlg();
    IupSetAttribute(filedlg, "DIALOGTYPE", "SAVE");
    IupSetAttribute(filedlg, "EXTFILTER", "BMP Files|*.bmp|");
    IupSetAttribute(filedlg, "TITLE", "Save BMP Image");

    IupPopup(filedlg, IUP_CENTER, IUP_CENTER);

    if (IupGetInt(filedlg, "STATUS") != -1) {
        char *filename = IupGetAttribute(filedlg, "VALUE");

        if (!image_save_bmp(g_image, filename)) {
            IupMessage("Error", "Could not save the image.");
        }
    }

    IupDestroy(filedlg);
    return IUP_DEFAULT;
}

static int cb_grayscale(Ihandle *self)
{
    (void)self;
    if (no_image_loaded()) return IUP_DEFAULT;
    save_undo();
    apply_grayscale(g_image);
    refresh_display();
    return IUP_DEFAULT;
}

static int cb_invert(Ihandle *self)
{
    (void)self;
    if (no_image_loaded()) return IUP_DEFAULT;
    save_undo();
    apply_invert(g_image);
    refresh_display();
    return IUP_DEFAULT;
}

static int cb_fliph(Ihandle *self)
{
    (void)self;
    if (no_image_loaded()) return IUP_DEFAULT;
    save_undo();
    apply_flip_horizontal(g_image);
    refresh_display();
    return IUP_DEFAULT;
}

static int cb_flipv(Ihandle *self)
{
    (void)self;
    if (no_image_loaded()) return IUP_DEFAULT;
    save_undo();
    apply_flip_vertical(g_image);
    refresh_display();
    return IUP_DEFAULT;
}

static int cb_brightness(Ihandle *self)
{
    (void)self;
    if (no_image_loaded()) return IUP_DEFAULT;

    int delta = 0;
    if (!IupGetParam("Brightness", NULL, NULL,
                      "Amount (-255 to 255): %i\n",
                      &delta, NULL))
        return IUP_DEFAULT; /* user cancelled */

    save_undo();
    apply_brightness(g_image, delta);
    refresh_display();
    return IUP_DEFAULT;
}

static int cb_rotate90(Ihandle *self)
{
    (void)self;
    if (no_image_loaded()) return IUP_DEFAULT;

    save_undo();
    Image *rotated = apply_rotate90(g_image);
    if (rotated) {
        image_free(g_image);
        g_image = rotated;
        refresh_display();
    }
    return IUP_DEFAULT;
}

static int cb_crop(Ihandle *self)
{
    (void)self;
    if (no_image_loaded()) return IUP_DEFAULT;

    int x = 0, y = 0, w = g_image->width, h = g_image->height;

    if (!IupGetParam("Crop Region", NULL, NULL,
                      "X: %i\n"
                      "Y: %i\n"
                      "Width: %i\n"
                      "Height: %i\n",
                      &x, &y, &w, &h, NULL))
        return IUP_DEFAULT; /* user cancelled */

    if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
        x + w > g_image->width || y + h > g_image->height) {
        IupMessage("Error", "The crop region is outside the image boundaries.");
        return IUP_DEFAULT;
    }

    save_undo();
    Image *cropped = apply_crop(g_image, x, y, w, h);
    if (cropped) {
        image_free(g_image);
        g_image = cropped;
        refresh_display();
    }
    return IUP_DEFAULT;
}

static int cb_blur(Ihandle *self)
{
    (void)self;
    if (no_image_loaded()) return IUP_DEFAULT;

    save_undo();
    Image *blurred = apply_blur(g_image);
    if (blurred) {
        image_free(g_image);
        g_image = blurred;
        refresh_display();
    }
    return IUP_DEFAULT;
}

static int cb_sharpen(Ihandle *self)
{
    (void)self;
    if (no_image_loaded()) return IUP_DEFAULT;

    save_undo();
    Image *sharpened = apply_sharpen(g_image);
    if (sharpened) {
        image_free(g_image);
        g_image = sharpened;
        refresh_display();
    }
    return IUP_DEFAULT;
}

static int cb_undo(Ihandle *self)
{
    (void)self;
    if (!g_undo_stack) {
        IupMessage("Info", "Nothing to undo.");
        return IUP_DEFAULT;
    }

    UndoNode *node = g_undo_stack;
    g_undo_stack = node->next;

    if (g_image) image_free(g_image);
    g_image = node->img;
    free(node);

    refresh_display();
    return IUP_DEFAULT;
}

/* ---------------- GUI construction ---------------- */

int main(int argc, char **argv)
{
    IupOpen(&argc, &argv);

    Ihandle *btn_open     = IupButton("Open",            NULL);
    Ihandle *btn_save     = IupButton("Save",            NULL);
    Ihandle *btn_gray     = IupButton("Grayscale",       NULL);
    Ihandle *btn_bright   = IupButton("Brightness...",   NULL);
    Ihandle *btn_invert   = IupButton("Invert",          NULL);
    Ihandle *btn_fliph    = IupButton("Flip Horizontal", NULL);
    Ihandle *btn_flipv    = IupButton("Flip Vertical",   NULL);
    Ihandle *btn_rotate   = IupButton("Rotate 90",       NULL);
    Ihandle *btn_crop     = IupButton("Crop...",         NULL);
    Ihandle *btn_blur     = IupButton("Blur",            NULL);
    Ihandle *btn_sharpen  = IupButton("Sharpen (bonus)", NULL);
    Ihandle *btn_undo     = IupButton("Undo",            NULL);

    IupSetCallback(btn_open,    "ACTION", (Icallback)cb_open);
    IupSetCallback(btn_save,    "ACTION", (Icallback)cb_save);
    IupSetCallback(btn_gray,    "ACTION", (Icallback)cb_grayscale);
    IupSetCallback(btn_bright,  "ACTION", (Icallback)cb_brightness);
    IupSetCallback(btn_invert,  "ACTION", (Icallback)cb_invert);
    IupSetCallback(btn_fliph,   "ACTION", (Icallback)cb_fliph);
    IupSetCallback(btn_flipv,   "ACTION", (Icallback)cb_flipv);
    IupSetCallback(btn_rotate,  "ACTION", (Icallback)cb_rotate90);
    IupSetCallback(btn_crop,    "ACTION", (Icallback)cb_crop);
    IupSetCallback(btn_blur,    "ACTION", (Icallback)cb_blur);
    IupSetCallback(btn_sharpen, "ACTION", (Icallback)cb_sharpen);
    IupSetCallback(btn_undo,    "ACTION", (Icallback)cb_undo);

    Ihandle *button_panel = IupVbox(
        btn_open, btn_save,
        IupLabel(""),
        btn_gray, btn_bright, btn_invert,
        btn_fliph, btn_flipv, btn_rotate,
        btn_crop, btn_blur, btn_sharpen,
        IupLabel(""),
        btn_undo,
        NULL);

    IupSetAttribute(button_panel, "MARGIN", "10x10");
    IupSetAttribute(button_panel, "GAP", "6");

    /* Create a blank placeholder image so the label starts already
       in "image mode" -- this avoids a redraw quirk where the very
       first switch from a text label to an image label does not show. */
    {
        int pw = 500, ph = 420;
        unsigned char *placeholder = (unsigned char*) malloc((size_t)pw * ph * 3);
        memset(placeholder, 230, (size_t)pw * ph * 3); /* light gray */
        g_iup_image = IupImageRGB(pw, ph, placeholder);
        free(placeholder);
    }

    g_canvas_label = IupLabel(NULL);
    IupSetAttributeHandle(g_canvas_label, "IMAGE", g_iup_image);
    IupSetAttribute(g_canvas_label, "RASTERSIZE", "500x420");
    IupSetAttribute(g_canvas_label, "EXPAND", "YES");

    Ihandle *main_hbox = IupHbox(button_panel, g_canvas_label, NULL);
    IupSetAttribute(main_hbox, "MARGIN", "5x5");

    g_dlg = IupDialog(main_hbox);
    IupSetAttribute(g_dlg, "TITLE", "Image Manipulation Software");
    IupSetAttribute(g_dlg, "SIZE", "750x480");

    IupShowXY(g_dlg, IUP_CENTER, IUP_CENTER);
    IupMainLoop();

    /* Cleanup */
    if (g_image) image_free(g_image);
    clear_undo_stack();
    IupClose();

    return 0;
}
