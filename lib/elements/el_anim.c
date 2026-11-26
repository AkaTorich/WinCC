// Анимации и таймеры Elements: плавности, трансформации (сдвиг/масштаб/поворот/
// прозрачность), появление элементов, периодические вызовы.
#include "el_internal.h"
#include <math.h>

double ease(int kind, double t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    switch (kind) {
    case EL_EASE_IN: return t * t;
    case EL_EASE_OUT: return 1 - (1 - t) * (1 - t);
    case EL_EASE_IN_OUT: return t < 0.5 ? 2 * t * t : 1 - pow(-2 * t + 2, 2) / 2;
    case EL_OVERSHOOT: { double c1 = 1.70158, c3 = c1 + 1, x = t - 1; return 1 + c3 * x * x * x + c1 * x * x; }
    case EL_BOUNCE: {
        double n1 = 7.5625, d1 = 2.75;
        if (t < 1 / d1) return n1 * t * t;
        else if (t < 2 / d1) { t -= 1.5 / d1; return n1 * t * t + 0.75; }
        else if (t < 2.5 / d1) { t -= 2.25 / d1; return n1 * t * t + 0.9375; }
        else { t -= 2.625 / d1; return n1 * t * t + 0.984375; } }
    default: return t;
    }
}

void el_animate(el e, const char *prop, int from, int to, int ms, int easing, int loop) {
    Anim *a;
    if (!valid(e) || !prop) return;
    a = &E(e)->anim;
    a->on = 1;
    strncpy(a->prop, prop, sizeof(a->prop) - 1); a->prop[sizeof(a->prop) - 1] = 0;
    a->from = from; a->to = to; a->ms = ms <= 0 ? 1 : ms; a->easing = easing; a->loop = loop;
    a->t0 = g.now_ms; a->dir = 1;
    request_redraw();
}

void el_appear(el e, const char *how, int ms) {
    if (!valid(e) || !how) return;
    if (!strcmp(how, "fade")) el_animate(e, "opacity", 0, 255, ms, EL_EASE_OUT, 0);
    else if (!strcmp(how, "slide") || !strcmp(how, "left")) el_animate(e, "x", -30, 0, ms, EL_EASE_OUT, 0);
    else if (!strcmp(how, "right")) el_animate(e, "x", 30, 0, ms, EL_EASE_OUT, 0);
    else if (!strcmp(how, "zoom")) el_animate(e, "scale", 70, 100, ms, EL_EASE_OUT, 0);
    else if (!strcmp(how, "pop")) el_animate(e, "scale", 0, 100, ms, EL_OVERSHOOT, 0);
    else if (!strcmp(how, "spin")) el_animate(e, "rotate", -120, 0, ms, EL_EASE_OUT, 0);
    else el_animate(e, "opacity", 0, 255, ms, EL_EASE_OUT, 0);
}

void el_transform(el e, int x, int y, int scale, int rotate, int opacity) {
    Element *m;
    if (!valid(e)) return;
    m = E(e);
    m->tx = x; m->ty = y; m->sc = scale; m->rot = rotate; m->op = opacity;
    request_redraw();
}

int el_animating(el e) { return valid(e) && E(e)->anim.on; }
void el_stop(el e) { if (valid(e)) E(e)->anim.on = 0; }

void transform_of(int h, double *tx, double *ty, double *sc, double *rot, double *op) {
    Element *e = E(h);
    double t, v;
    *tx = e->tx; *ty = e->ty; *sc = e->sc; *rot = e->rot; *op = e->op;
    if (!e->anim.on) return;
    t = (g.now_ms - e->anim.t0) / e->anim.ms;
    if (e->anim.loop == 0) { if (t > 1) t = 1; }
    else if (e->anim.loop == 1) t = t - floor(t);
    else { double cyc = fmod(t, 2.0); t = cyc <= 1 ? cyc : 2 - cyc; }
    v = e->anim.from + (e->anim.to - e->anim.from) * ease(e->anim.easing, t);
    if (!strcmp(e->anim.prop, "opacity")) *op = v;
    else if (!strcmp(e->anim.prop, "x")) *tx = v;
    else if (!strcmp(e->anim.prop, "y")) *ty = v;
    else if (!strcmp(e->anim.prop, "scale")) *sc = v;
    else if (!strcmp(e->anim.prop, "rotate")) *rot = v;
}

void anim_frame(void) {
    int i;
    // завершение одноразовых анимаций
    for (i = 1; i < g.el.n; i++) {
        Element *e = E(i);
        if (!e->alive || !e->anim.on) continue;
        if (e->anim.loop == 0 && g.now_ms - e->anim.t0 >= e->anim.ms) {
            // зафиксировать конечное значение в базе
            if (!strcmp(e->anim.prop, "opacity")) e->op = e->anim.to;
            else if (!strcmp(e->anim.prop, "x")) e->tx = e->anim.to;
            else if (!strcmp(e->anim.prop, "y")) e->ty = e->anim.to;
            else if (!strcmp(e->anim.prop, "scale")) e->sc = e->anim.to;
            else if (!strcmp(e->anim.prop, "rotate")) e->rot = e->anim.to;
            e->anim.on = 0;
        }
    }
    // таймеры (вызов может добавить новый таймер — массив перечитывается)
    for (i = 0; i < g.timers.n; i++) {
        Timer *t = &g.timers.a[i];
        if (t->dead) continue;
        if (g.now_ms >= t->next) {
            el_fn fn = t->fn;
            void *arg = t->arg;
            t->next = g.now_ms + t->ms;
            if (fn) fn(0, arg);
            request_redraw();
        }
    }
    // уборка мёртвых таймеров
    for (i = 0; i < g.timers.n;) { if (g.timers.a[i].dead) VERASE(g.timers, i); else i++; }
}

int anim_busy(void) {
    int i;
    for (i = 1; i < g.el.n; i++) if (E(i)->alive && E(i)->anim.on) return 1;
    for (i = 0; i < g.timers.n; i++) if (!g.timers.a[i].dead) return 1;
    return 0;
}

int el_every(int ms, el_fn fn, void *arg) {
    Timer t;
    t.id = ++g.timer_seq; t.ms = ms <= 0 ? 1 : ms; t.next = g.now_ms + t.ms; t.fn = fn; t.arg = arg; t.dead = 0;
    VPUSH(g.timers, t);
    return t.id;
}
void el_timer_stop(int id) { int i; for (i = 0; i < g.timers.n; i++) if (g.timers.a[i].id == id) g.timers.a[i].dead = 1; }
