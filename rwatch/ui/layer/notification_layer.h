#pragma once
/* notification_window.h
 *
 * Displays notifications sent to the watch from the phone.
 *
 * Author: Carson Katri <me@carsonkatri.com>
 */

#include "librebble.h"

typedef enum NotificationAction
{
    NotificationActionDismiss = 0,
    NotificationActionDismissAll = 1,
    NotificationActionReply = 2,
    NotificationActionCustom = 3
} NotificationAction;

typedef struct Notification
{
    GBitmap *icon;
    const char *app_name;
    const char *title;
    const char *body;
    char *custom_actions;
    GColor color;
    
    list_node node;
} Notification;

typedef struct NotificationLayer
{
    Layer layer;
    NotificationAction *actions;
    Notification *active;
    list_head notif_list_head;
} NotificationLayer;

Notification* notification_create(const char *app_name, const char *title, const char *body, GBitmap *icon, GColor color);

static void notification_layer_ctor(NotificationLayer *notification_layer, GRect frame);
static void notification_layer_dtor(NotificationLayer *notification_layer);
NotificationLayer *notification_layer_create(GRect frame);
Layer* notification_layer_get_layer(NotificationLayer *notification_layer);
void notification_layer_destroy(NotificationLayer *notification_layer);
Notification* notification_create(const char *app_name, const char *title, const char *body, GBitmap *icon, GColor color);
void notification_layer_stack_push_notification(NotificationLayer *notification_layer, Notification *notification);
void notification_layer_configure_click_config(NotificationLayer *notification_layer, Window *window);
