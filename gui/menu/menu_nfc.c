#include "menu_connection.h"

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_settings.h"
#include "ui/ui_anim.h"

#include "nfc/nfc_file.h"

#ifdef VANILLA_LIBNFC_AVAILABLE
#include "nfc/nfc_libnfc.h"
#endif

const VanillaNfcBackend *nfc_backends[] = {
    &nfc_file_backend,
#ifdef VANILLA_LIBNFC_AVAILABLE
    &nfc_libnfc_backend,
#endif
};

#define NUM_BACKENDS (sizeof(nfc_backends) / sizeof(nfc_backends[0]))
size_t nfc_num_backends = NUM_BACKENDS;

static int bglayer;
static int fglayer;
static int nfc_backend_btns[NUM_BACKENDS];

static void return_to_settings(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, bglayer, vpi_menu_settings, 0);
}

static void nfc_backend_clicked(vui_context_t *vui, int btn, void *v)
{
    int reg = (intptr_t) v;

    vpi_config.nfc_backend = reg;
    vpi_config_save();

    for (int i = 0; i < NUM_BACKENDS; i++) {
        int b = nfc_backend_btns[i];
        vui_button_update_checked(vui, b, vpi_config.region == i);
    }

    return_to_settings(vui, btn, 0);
}

void vpi_menu_nfc(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    bglayer = vui_layer_create(vui);
    fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    const int lbl_margin = margin * 2;
    const int lbl_y = bkg_rect.y + lbl_margin;
    vui_label_create(vui, bkg_rect.x + lbl_margin, lbl_y, bkg_rect.w - lbl_margin - lbl_margin,
                     bkg_rect.h - lbl_margin - lbl_margin, lang(VPI_LANG_NFC_HELP), vui_color_create(1, 1, 1, 1),
                     VUI_FONT_SIZE_NORMAL, fglayer);

    const int btn_w = bkg_rect.w / 2;
    const int btn_y = bkg_rect.y + bkg_rect.h * 13 / 32;
    const int btn_x = bkg_rect.x + bkg_rect.w / 2 - btn_w / 2;

    for (int i = 0; i < NUM_BACKENDS; i++) {
        int b = vui_button_create(vui, btn_x, btn_y + BTN_SZ * i, btn_w, BTN_SZ, lang(VPI_LANG_NFC_BACKEND_FILE + i), 0,
                                  VUI_BUTTON_STYLE_BUTTON, fglayer, nfc_backend_clicked, (void *) (intptr_t) i);
        vui_button_update_checkable(vui, b, 1);
        vui_button_update_checked(vui, b, vpi_config.nfc_backend == i);
        nfc_backend_btns[i] = b;
    }

    // Back button
    vpi_menu_create_back_button(vui, fglayer, return_to_settings, (void *) (intptr_t) bglayer);

    vui_transition_fade_layer_in(vui, bglayer, 0, 0);
}