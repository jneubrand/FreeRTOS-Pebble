/* window.c
 * routines for [...]
 * libRebbleOS
 *
 * Author: Barry Carter <barry.carter@gmail.com>
 *         Carson Katri <me@carsonkatri.com>
 */

#include "librebble.h"
#include "ngfxwrap.h"
#include "node_list.h"
#include "property_animation.h"
#include "animation.h"

static list_head _window_list_head = LIST_HEAD(_window_list_head);

void _window_unload_proc(Window *window);
void _window_load_proc(Window *window);
void _window_load_click_config(Window *window);

bool _anim_direction_left = true;

/*
 * Create a new top level window and all of the contents therein
 */
Window *window_create(void)
{
    Window *window = calloc(1, sizeof(Window));
    
    if (window == NULL)
    {
        SYS_LOG("window", APP_LOG_LEVEL_ERROR, "No memory for Window");
        return NULL;
    }
    // and it's root layer
    GRect bounds = GRect(0, 0, DISPLAY_COLS, DISPLAY_ROWS);
    GRect frame = GRect(0, 0, DISPLAY_COLS, DISPLAY_ROWS);
    window->frame = frame;
    window->root_layer = layer_create(bounds);
    window->background_color = GColorWhite;
    window->is_loaded = false;

    return window;
}

/*
 * Set the pointers to the functions to call when the window is shown
 */
void window_set_window_handlers(Window *window, WindowHandlers handlers)
{
    if (window == NULL)
        return;
    
    window->window_handlers = handlers;
}

static void push_animation_setup(Animation *animation) {
    SYS_LOG("window", APP_LOG_LEVEL_INFO, "Anim window ease in.");
}

static void push_animation_update(Animation *animation,
                                  const AnimationProgress progress) {
    Window *window = window_stack_get_top_window(); 
    int existx = window->frame.origin.x; 
    int newx, delta; 
     
    if (*((bool*)animation->context) == true) 
    { 
        newx = GLERP(DISPLAY_COLS, 0, progress); 
        delta = (existx - newx); 
        animation_util_push_fb(existx, delta, true);
    } 
    else 
    { 
        newx = GLERP(-DISPLAY_COLS, 0, progress); 
        delta = newx - existx;
        animation_util_push_fb(DISPLAY_COLS - (existx + DISPLAY_COLS), delta, false);
    } 

    window->frame.origin.x = newx; 
    window_dirty(true); 
    window_draw(); 
}

static void push_animation_teardown(Animation *animation) {
    SYS_LOG("window", APP_LOG_LEVEL_INFO, "Animation finished!");
    animation_destroy(animation);
}

/*
 * Push a window onto the main window window_stack_push
 */
void window_stack_push(Window *window, bool animated)
{
    list_init_node(&window->node);
    list_insert_head(&_window_list_head, &window->node);
    if (animated)
    {
         Window *window = window_stack_get_top_window();
        window->frame.origin.x = DISPLAY_COLS;
        App *app = appmanager_get_running_app();
        /* A quicky hack to determine direction of scroll
         * If we are an app => face, then we go left
         * Face to app => right
         */
        if (app->type == APP_TYPE_FACE)
            _animation_setup(true);
        else 
            _animation_setup(false);
    }
    window_configure(window);
    window_dirty(true);
    window_count();
}

void _animation_setup(bool direction_left)
{
    // Animate the window change
    Animation *animation = animation_create();
    animation_set_duration(animation, 300);
    
    const AnimationImplementation implementation = {
        .setup = push_animation_setup,
        .update = push_animation_update,
        .teardown = push_animation_teardown
    };
    animation_set_implementation(animation, &implementation);
 
    _anim_direction_left = direction_left;
    animation->context = (void*)&_anim_direction_left;
    
    // Play the animation
    animation_schedule(animation);
}

/*
 * Remove the top_window from the list
 */
Window * window_stack_pop(bool animated)
{
    Window *wind = window_stack_get_top_window();
    window_stack_remove(wind, animated);
    
    Window *newwind = window_stack_get_top_window();

    if (newwind)
        window_configure(newwind);

    return wind;
}

/*
 * Remove a window from the list
 */
bool window_stack_remove(Window *window, bool animated)
{
    list_remove(&_window_list_head, &window->node);

    return true;
}

/*
 * Get the topmost window
 */
Window * window_stack_get_top_window(void)
{
    return list_elem(list_get_head(&_window_list_head), Window, node);
}

uint16_t window_count(void)
{
    uint16_t count = 0;
    Window *w;
    
    if (list_get_head(&_window_list_head) == NULL)
        return 0;
    
    list_foreach(w, &_window_list_head, Window, node)
    {
        count++;
    }
    
    SYS_LOG("window", APP_LOG_LEVEL_INFO, "COUNT %d", count);
    
    return count;
}

/*
 * Check if a window exists in the stack
 */
bool window_stack_contains_window(Window *window)
{
    Window *w;
    list_foreach(w, &_window_list_head, Window, node)
        if (window == w)
            return true;
    
    return false;
}

/*
 * Kill a window. Clean up references and memory
 */
void window_destroy(Window *window)
{
    list_remove(&_window_list_head, &window->node);
    SYS_LOG("window", APP_LOG_LEVEL_INFO, "Destroy!");
    window_count();
    /* Unload the window */
    _window_unload_proc(window);
    /* free all of the layers */
    layer_destroy(window->root_layer);
    /* and now the window */
    app_free(window);
    window = window_stack_get_top_window();
    if (!window)
    {
        SYS_LOG("window", APP_LOG_LEVEL_INFO, "No more windows!");
        return;
    }
    
    /* Clean up the clink handler and remap back to our current window */
    _window_load_click_config(window);
    window_draw();
    window_dirty(true);
}

/*
 * Get the top level window's layer
 */
Layer *window_get_root_layer(Window *window)
{
    if (window == NULL)
        return NULL;
    
    return window->root_layer;
}


/*
 * Invalidate the window so it is scheduled for a redraw
 */
void window_dirty(bool is_dirty)
{
    Window *wind = window_stack_get_top_window();
    
    if (wind == NULL)
        return;
    
    if (wind->is_render_scheduled != is_dirty)
    {
        wind->is_render_scheduled = is_dirty;
        
        if (is_dirty)
            appmanager_post_draw_message();
    }
}

void window_draw()
{
    Window *wind = window_stack_get_top_window();
    
    if (wind == NULL)
        return;
    if (wind && wind->is_render_scheduled)
    {
        GContext *context = rwatch_neographics_get_global_context();
        GRect frame = layer_get_frame(wind->root_layer);
        /* Apply window offset too */
        GRect windowframe = wind->frame; 
        frame.origin.x += windowframe.origin.x; 
        frame.origin.y += windowframe.origin.y; 
        context->offset = frame;
        context->fill_color = wind->background_color;
        graphics_fill_rect(context, GRect(0, 0, frame.size.w, frame.size.h), 0, GCornerNone);
        
        layer_draw(wind->root_layer, context);
        
        rbl_draw();
        wind->is_render_scheduled = false;
    }
}

/*
 * Window click config provider registration implementation.
 * For the most part, these will just defer to the button recogniser
 */

void window_set_click_config_provider(Window *window, ClickConfigProvider click_config_provider)
{
    window->click_config_provider = click_config_provider;
}

void window_set_click_config_provider_with_context(Window *window, ClickConfigProvider click_config_provider, void *context)
{
    window->click_config_provider = click_config_provider;
    window->click_config_context = context;
}

ClickConfigProvider window_get_click_config_provider(const Window *window)
{
    return window->click_config_provider;
}

void *window_get_click_config_context(Window *window)
{
    return window->click_config_context;
}

void window_set_background_color(Window *window, GColor background_color)
{
    window->background_color = background_color;
}

bool window_is_loaded(Window *window)
{
    return window->is_loaded;
}

void window_set_user_data(Window *window, void *data)
{
    window->user_data = data;
}

void * window_get_user_data(const Window *window)
{
    return window->user_data;
}

void window_single_click_subscribe(ButtonId button_id, ClickHandler handler)
{
    button_single_click_subscribe(button_id, handler);
}

void window_single_repeating_click_subscribe(ButtonId button_id, uint16_t repeat_interval_ms, ClickHandler handler)
{
    button_single_repeating_click_subscribe(button_id, repeat_interval_ms, handler);
}

void window_multi_click_subscribe(ButtonId button_id, uint8_t min_clicks, uint8_t max_clicks, uint16_t timeout, bool last_click_only, ClickHandler handler)
{
    button_multi_click_subscribe(button_id, min_clicks, max_clicks, timeout, last_click_only, handler);
}

void window_long_click_subscribe(ButtonId button_id, uint16_t delay_ms, ClickHandler down_handler, ClickHandler up_handler)
{
    button_long_click_subscribe(button_id, delay_ms, down_handler, up_handler);
}

void window_raw_click_subscribe(ButtonId button_id, ClickHandler down_handler, ClickHandler up_handler, void * context)
{
    button_raw_click_subscribe(button_id, down_handler, up_handler, context);
}

void window_set_click_context(ButtonId button_id, void *context)
{
    button_set_click_context(button_id, context);
}


/*
 * Deal with window level callbacks when a window is created.
 * These will call through to the pointers to the functions in
 * the user supplied window
 */
void window_configure(Window *window)
{
    // we assume they are configured now
    _window_load_proc(window);
    _window_load_click_config(window);
}

/* 
 * Call the window's _load handler and flag as loaded
 */
void _window_load_proc(Window *window)
{
    if (window->is_loaded)
        return;
         
    if (window->window_handlers.load) 
    window->window_handlers.load(window); 
         
    /* we should flag as loaded even if they don't have a load handler */ 
    window->is_loaded = true; 
    return;
}

/* 
 * Call the window's _unload handler and flag as unloaded
 */
void _window_unload_proc(Window *window) 
{ 
    if (!window->is_loaded)
        return;
    
    if (window->window_handlers.unload)
        window->window_handlers.unload(window);
    
    /* we should flag as unloaded even if they don't have a load handler */
    window->is_loaded = false;
    return;
}

void _window_load_click_config(Window *window) 
{    
    if (window->click_config_provider) { 
        void* context = window->click_config_context ? window->click_config_context : window; 
        window->click_config_provider(context); 
    } 
}


/* This prob shouldn't be in here, but I feel it doesn't live in 
   animation either */

/* 
 * Grab the screenbuffer and push it off the screen left or right by n
 * pixels.
 */
void animation_util_push_fb(int width, int distance, bool left)
{
    /* XXX don't go to hw layer here. capture the context */ 
    uint8_t *fb = hw_display_get_buffer(); 
    if (left) 
    { 
        /* push the framebuffer out left.
         * For every row get the beginning of the row
         * and copy byte by byte
         */ 
        for (uint16_t i = 0; i < DISPLAY_ROWS; i++) 
        { 
            /* row start */ 
            int rs = DISPLAY_COLS * i; 
             
            for (int j = 0; j < width; j++) 
                fb[rs + j] = fb[rs + j + distance]; 
        } 
    } 
    else 
    { 
        /* Push the framebuffer out right
         * We get the row end and work inwards
         */
        for (uint16_t i = 0; i < DISPLAY_ROWS; i++) 
        { 
            /* row end. We will work backwards */ 
            int rs = DISPLAY_COLS * i + DISPLAY_COLS - 1; 
             
            for (int j = 0; j < width; j++) 
                fb[rs - j] = fb[rs - j - distance]; 
        } 
    } 
}
