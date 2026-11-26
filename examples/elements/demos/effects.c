// Витрина эффектов окна: слева — живой интерфейс, справа — переключатели
// эффектов. Эффект применяется ко всему готовому кадру (постобработка).
#include <elements.h>
#include <stdint.h>

const char *FX[]    = { "chromatic", "scanlines", "noise", "vignette", "glitch", "bloom", "pixelate", "grayscale", "sepia", "invert" };
const char *NAMES[] = { "Dispersion", "Scanlines", "Noise", "Vignette", "Glitch", "Glow (bloom)", "Pixelate", "Grayscale", "Sepia", "Invert" };
int AMT[]           = { 3, 30, 18, 40, 16, 55, 8, 100, 100, 100 };
int on[10]          = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };   // по умолчанию выключено — текст читаемый; включайте флажками
int level = 70;

void onfx(el e, void *a) { int i = (int)(intptr_t)a; el_effect(FX[i], on[i] ? AMT[i] : 0); }

void preview(el e, int w, int h, void *arg)
{
    // яркое неоновое содержимое, чтобы эффекты были видны
    el_g_round(0, 0, w, h, 14, 0x0c1020);
    for (int i = 0; i < 6; i++) el_g_ring(w / 2, 90, 20 + i * 14, 2, 0x00e0ff);
    el_g_disc(70, 80, 34, 0xff2e88);
    el_g_disc(w - 70, 80, 34, 0xffe600);
    for (int x = 20; x < w - 20; x++) { int c = ((x - 20) * 255 / (w - 40)); el_g_fill(x, 170, 1, 16, (c << 16) | (120 << 8) | (255 - c)); }
    el_g_text(w / 2 - el_g_text_w("NEON", 44, 1) / 2, 200, "NEON", 0xffffff, 44, 1);
    el_g_text(w / 2 - el_g_text_w("Window effects", 18, 1) / 2, 256, "Window effects", 0x8fe0ff, 18, 1);
}

int main(void)
{
    el w = el_window("Effects", 760, 520);
    el_theme(1);

    el root = el_grid(w, "*,250", "*");
    el_gap(root, 0);

    el left = el_border(root);
    el_cell(left, 0, 0, 1, 1);
    el_style(left, "background:#0a0e1a; padding:16 16");
    el ls = el_stack(left, 0);
    el_gap(ls, 0);
    el pv = el_paint(ls, preview, 0);
    el_size(pv, -1, 300);
    el bar = el_stack(ls, 1);
    el_margin(bar, 0, 14, 0, 0);
    el_gap(bar, 10);
    el_align(bar, EL_CENTER, EL_START);
    el_style(el_button(bar, "Button"), "gradient:#ff2e88 #7a3dff horizontal; color:#fff; font-weight:bold; radius:8; padding:8 20");
    el sl = el_slider(bar, 0, 100, &level);
    el_size(sl, 180, -1);

    el panel = el_border(root);
    el_cell(panel, 0, 1, 1, 1);
    el_style(panel, "background:#10131f; border:#2a3350; border-width:1; padding:16 18");
    el ps = el_stack(panel, 0);
    el_gap(ps, 12);
    el ttl = el_text(ps, "Window effects");
    el_style(ttl, "font-size:17; font-weight:bold; color:#eaf2ff");
    for (int i = 0; i < 10; i++) {
        el c = el_check(ps, NAMES[i], &on[i]);
        el_on_change(c, onfx, (void *)(intptr_t)i);
    }
    el hint = el_text(ps, "Toggle an effect and it applies to the whole window. Several can be on at once.");
    el_wrap_text(hint, 1);
    el_style(hint, "color:#6f7ca0; font-size:12");

    // начальный набор
    for (int i = 0; i < 10; i++) if (on[i]) el_effect(FX[i], AMT[i]);

    el_run();
    return 0;
}
