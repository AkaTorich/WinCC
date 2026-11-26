// Самый простой пример Elements: поле, кнопка, подпись.
#include <elements.h>
#include <stdio.h>

char name[64] = "World";
char out[128] = "Press the button.";

void hi(el e, void *arg) { sprintf(out, "Hello, %s!", name[0] ? name : "friend"); }

int main(void)
{
    el w = el_window("Hello, Elements", 420, 230);
    el s = el_stack(w, 0);
    el_padding(s, 22, 22, 22, 22);
    el_gap(s, 12);

    el_text(s, "What is your name?");
    el e = el_edit(s, name, sizeof(name));
    el_tooltip(e, "Type a name and press the button");
    el_on_click(e, hi, 0);                 // Enter в поле — тоже «привет»
    el_on_click(el_button(s, "Say hello"), hi, 0);

    el t = el_text(s, "");
    el_text_bind(t, out);
    el_font(t, 17, 1);

    el_run();
    return 0;
}
