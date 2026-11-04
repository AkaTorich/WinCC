// Показ WUI: страницы (pages.c) и окно (main.c).
#pragma once
#include <wui.h>

enum { PAGE_COUNT = 5 };

WuiControl *page_controls(WuiControl *parent);
WuiControl *page_lists(WuiControl *parent);
WuiControl *page_cards(WuiControl *parent);
WuiControl *page_canvas(WuiControl *parent);
WuiControl *page_about(WuiControl *parent);
