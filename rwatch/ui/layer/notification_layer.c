/* notification_layer.c
 *
 * Displays notifications sent to the watch from the phone.
 *
 * Author: Carson Katri <me@carsonkatri.com>
 */

#include <stdbool.h>
#include "notification_layer.h"
#include "action_menu.h"
#include "status_bar_layer.h"
#include "librebble.h"
#include "ngfxwrap.h"

static StatusBarLayer *_status_bar;

static void _notification_layer_load(Window *window);
static void _notification_layer_unload(Window *window);
static void _notification_layer_update_proc(Layer *layer, GContext *ctx);
static void _click_config_provider(void *context);

static void notification_layer_ctor(NotificationLayer *notification_layer, GRect frame)
{
    layer_ctor(&notification_layer->layer, frame);
    list_init_head(&notification_layer->notif_list_head);
    layer_set_update_proc(&notification_layer->layer, _notification_layer_update_proc);
}

static void notification_layer_dtor(NotificationLayer *notification_layer)
{
    /* Free the chain */
    Notification *n;
    list_node *l = list_get_head(&notification_layer->notif_list_head);
    while(l)
    {
        n = list_elem(l, Notification, node);
        list_remove(&notification_layer->notif_list_head, &n->node);
        app_free(n);
        
        SYS_LOG("noty", APP_LOG_LEVEL_ERROR, "Deleted all Notifications");
        break;
    }
        
    layer_destroy(&notification_layer->layer);
    app_free(notification_layer);
    window_dirty(true);
}

NotificationLayer *notification_layer_create(GRect frame)
{
    SYS_LOG("noty", APP_LOG_LEVEL_INFO, "Create");

    /* Make the window */
    NotificationLayer *notification_layer = app_calloc(1, sizeof(NotificationLayer));
    notification_layer_ctor(notification_layer, frame);
    SYS_LOG("noty", APP_LOG_LEVEL_INFO, "Create %d", &notification_layer->layer);
        
    return notification_layer;
}

void notification_layer_destroy(NotificationLayer *notification_layer)
{
    notification_layer_dtor(notification_layer);    
}

Notification* notification_create(const char *app_name, const char *title, const char *body, GBitmap *icon, GColor color)
{
    Notification *notification = app_calloc(1, sizeof(Notification));
    
    if (notification == NULL)
    {
        SYS_LOG("notification_layer", APP_LOG_LEVEL_ERROR, "No memory for Notification");
        return NULL;
    }
    
    notification->app_name = app_name;
    notification->title = title;
    notification->body = body;
    notification->icon = icon;
    notification->color = color;
    
    return notification;
}


void notification_layer_stack_push_notification(NotificationLayer *notification_layer, Notification *notification)
{   
    list_init_node(&notification->node);
    list_insert_head(&notification_layer->notif_list_head, &notification->node);
    notification_layer->active = notification;
    
    layer_mark_dirty(&notification_layer->layer);
}

Layer* notification_layer_get_layer(NotificationLayer *notification_layer)
{
    SYS_LOG("noty", APP_LOG_LEVEL_INFO, "adasdasdsasas Create %d", &notification_layer->layer);
    return &notification_layer->layer;
}

void notification_layer_configure_click_config(NotificationLayer *notification_layer, Window *window)
{
    window_set_click_config_provider_with_context(window, 
                                                  (ClickConfigProvider)_click_config_provider, 
                                                  notification_layer);
}



static void _scroll_up_click_handler(ClickRecognizerRef recognizer, void *context)
{
    NotificationLayer *notification_layer = (NotificationLayer *)context;
    Notification *notification = notification_layer->active;
        
    if (notification->node.prev != &notification_layer->notif_list_head)
    {
        /* Show the next notification on the stack */
        Notification *n = list_elem(notification->node.prev, Notification, node);
        notification_layer->active = n;
        
#ifdef PBL_RECT
        status_bar_layer_set_colors(_status_bar, notification->color, GColorWhite);
#else
        status_bar_layer_set_colors(_status_bar, GColorWhite, GColorBlack);
#endif
    }
    else
    {
        status_bar_layer_set_colors(_status_bar, notification->color, GColorWhite);
    }
    
    window_dirty(true);
}

static void _scroll_down_click_handler(ClickRecognizerRef recognizer, void *context)
{
    NotificationLayer *notification_layer = (NotificationLayer *)context;
    Notification *notification = notification_layer->active;
    
    if (notification->node.next != &notification_layer->notif_list_head)
    {
        /* Show the previous notification on the stack */
        Notification *n = list_elem(notification->node.next, Notification, node);
        notification_layer->active = n;

        SYS_LOG("notification_layer", APP_LOG_LEVEL_DEBUG, notification_layer->active->app_name);
        
        // Scroll to top
        status_bar_layer_set_colors(_status_bar, notification_layer->active->color, GColorWhite);
    }
    else
    {        
#ifndef PBL_RECT
        _status_bar_layer_set_colors(_status_bar, GColorWhite, GColorBlack);
#endif
    }
    
    window_dirty(true);
}

static void _action_performed_callback(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
    // ACTION!
}

static void _show_options_click_handler(ClickRecognizerRef recognizer, void *context)
{
    NotificationLayer *notification_layer = (NotificationLayer *)context;
    Notification *notification = notification_layer->active;
    
    // Make + show the ActionMenu
    ActionMenuLevel *root_level = action_menu_level_create(3);
    ActionMenuLevel *reply_level = action_menu_level_create(3);
    
    action_menu_level_add_child(root_level, reply_level, "Reply");
    action_menu_level_add_action(root_level, "Dismiss", _action_performed_callback, NULL);
    action_menu_level_add_action(root_level, "Dismiss All", _action_performed_callback, NULL);
    
    action_menu_level_add_action(reply_level, "Dictate", _action_performed_callback, NULL);
    action_menu_level_add_action(reply_level, "Canned Response", _action_performed_callback, NULL);
    action_menu_level_add_action(reply_level, "Emoji", _action_performed_callback, NULL);
    
    ActionMenuConfig config = (ActionMenuConfig)
    {
        .root_level = root_level,
        .colors = {
            .background = notification_layer->active->color,
            .foreground = GColorWhite,
        },
        .align = ActionMenuAlignCenter
    };
    
    action_menu_open(&config);
}

static void _pop_notification_click_handler(ClickRecognizerRef recognizer, void *context)
{
  /* XXX pop the window, call the unload handler on the notification_layer */
}

static void _click_config_provider(void *context)
{
    window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) _scroll_down_click_handler);
    window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) _scroll_up_click_handler);
    window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) _show_options_click_handler);
    window_single_click_subscribe(BUTTON_ID_BACK, (ClickHandler) _pop_notification_click_handler);
    
    window_set_click_context(BUTTON_ID_DOWN, context);
    window_set_click_context(BUTTON_ID_UP, context);
    window_set_click_context(BUTTON_ID_SELECT, context);
}

/*
static void _notification_layer_load(Window *window)
{
    Layer *layer = window_get_root_layer(window);
    
    GRect bounds = layer_get_unobstructed_bounds(layer);
    
    Layer *main_layer = layer_create(bounds);
    layer_set_update_proc(main_layer, _notification_layer_update_proc);
    layer_add_child(layer, main_layer);
    
    // Set the click config provider
    window_set_click_config_provider_with_context(window, 
                                                  (ClickConfigProvider)_click_config_provider, 
                                                  (NotificationLayer *)window->context);
    
    // Status Bar
    _status_bar = status_bar_layer_create();
    NotificationLayer *notification_layer = (NotificationLayer *)window->context;
    status_bar_layer_set_colors(_status_bar, notification_layer->active->color, GColorWhite);
    layer_add_child(main_layer, status_bar_layer_get_layer(_status_bar));
    
    layer_mark_dirty(layer);
    window_dirty(true);
}
*/

static void _notification_layer_update_proc(Layer *layer, GContext *ctx)
{
    NotificationLayer *notification_layer = container_of(layer, NotificationLayer, layer);
    assert(notification_layer);
    Notification *notification = notification_layer->active;
    if (!notification)
        return;
    int offset = 0;//-4 + notification_layer->offset; // -4 because of the _status_bar
    GRect bounds = layer_get_unobstructed_bounds(layer);
    
    GBitmap *icon = notification->icon;
    char *app = notification->app_name;
    char *title = notification->title;
    char *body = notification->body;
    
    SYS_LOG("noty", APP_LOG_LEVEL_DEBUG, "XXXXXXX %d %d", notification_layer, layer);
    
    // Draw the background:
    graphics_context_set_fill_color(ctx, notification->color);
#ifdef PBL_RECT
    GRect rect_bounds = GRect(0, 0 - offset, bounds.size.w, 35);
    graphics_fill_rect(ctx, rect_bounds, 0, GCornerNone);
#else
    n_graphics_fill_circle(ctx, GPoint(DISPLAY_COLS / 2, (-DISPLAY_COLS + 35) - offset), DISPLAY_COLS);
#endif
    
    // Draw the icon:
    if (icon != NULL)
    {
        GSize icon_size = icon->raw_bitmap_size;
        graphics_draw_bitmap_in_rect(ctx, icon, GRect(bounds.size.w / 2 - (icon_size.w / 2), 17 - offset - (icon_size.h / 2), icon_size.w, icon_size.h));
    }
    
#ifdef PBL_RECT
    GRect app_rect = GRect(10, 35 - offset, bounds.size.w - 20, 20);
    GRect title_rect = GRect(10, 52 - offset, bounds.size.w - 20, 30);
    GRect body_rect = GRect(10, 67 - offset, bounds.size.w - 20, (DISPLAY_ROWS * 2) - 67);
    
    GTextAlignment alignment = GTextAlignmentLeft;
#else
    GRect app_rect = GRect(0, 35 - offset, bounds.size.w, 20);
    
    GRect title_rect = GRect(0, 52 - offset, bounds.size.w, 20);
    
    //GSize body_size = n_graphics_text_layout_get_content_size(body, body_font);
    GRect body_rect = GRect(0, 67 - offset, bounds.size.w, (DISPLAY_ROWS * 2) - 50);
    
    GTextAlignment alignment = GTextAlignmentCenter;
#endif
    
    // Draw the app:
    ctx->text_color = notification->color;
    graphics_draw_text(ctx, app, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), app_rect, GTextOverflowModeTrailingEllipsis, alignment, 0);
    
    // Draw the title:
    ctx->text_color = GColorBlack;
    graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), title_rect, GTextOverflowModeTrailingEllipsis, alignment, 0);
    
    // Draw the body:
    graphics_draw_text(ctx, body, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), body_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, 0);
    
    // Draw the indicator:
    graphics_context_set_fill_color(ctx, GColorBlack);
    n_graphics_fill_circle(ctx, GPoint(DISPLAY_COLS + 2, DISPLAY_ROWS / 2), 10);
    
    // And the other one:
#ifndef PBL_RECT
    if (notification->previous != NULL && notification_layer->offset > 0)
    {
        graphics_context_set_fill_color(ctx, notification->previous->color);
        n_graphics_fill_circle(ctx, GPoint(DISPLAY_COLS / 2, DISPLAY_ROWS - 2), 10);
    }
#endif
}

static void _notification_layer_unload(Window *window)
{

}
