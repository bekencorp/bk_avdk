// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.0
// LVGL version: 8.3.3
// Project name: AiotArc

#include "../ui.h"

int temp = 17;

void ui_jianbutton_event(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    char buff[8];

    if(event_code == LV_EVENT_CLICKED) 
    {
        if(temp > 17)
            temp--;
        
        lv_snprintf(buff, sizeof(buff), "%d", temp);
        lv_label_set_text(ui_Label2, buff);
    }
}

void ui_jiabutton_event(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    char buff[8];

    if(event_code == LV_EVENT_CLICKED) 
    {
        if(temp < 30)
            temp++;
        
        lv_snprintf(buff, sizeof(buff), "%d", temp);
        lv_label_set_text(ui_Label2, buff);
    }
}

void ui_UiArcPage_screen_init(lv_obj_t *parent)
{
    char buff[8];
    
    ui_UiArcPage = lv_obj_create(parent);
    lv_obj_clear_flag(ui_UiArcPage, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_UiArcPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_UiArcPage, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_UiArcPage, lv_color_hex(0x2D323C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(ui_UiArcPage, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_border_width(ui_UiArcPage, 0, 0);
    lv_obj_set_style_pad_all(ui_UiArcPage, 0, 0);
    lv_obj_set_style_radius(ui_UiArcPage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_UAPColdImg = lv_img_create(ui_UiArcPage);
    lv_img_set_src(ui_UAPColdImg, &ui_img_cold_symbol_png);
    lv_obj_set_width(ui_UAPColdImg, LV_SIZE_CONTENT);   /// 55
    lv_obj_set_height(ui_UAPColdImg, LV_SIZE_CONTENT);    /// 62
    lv_obj_set_x(ui_UAPColdImg, 30);
    lv_obj_set_y(ui_UAPColdImg, -20);
    lv_obj_set_align(ui_UAPColdImg, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_add_flag(ui_UAPColdImg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_UAPColdImg, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_img_recolor(ui_UAPColdImg, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_img_recolor_opa(ui_UAPColdImg, 50, LV_PART_MAIN | LV_STATE_PRESSED);

    ui_WAPSwitchImg = lv_img_create(ui_UiArcPage);
    lv_img_set_src(ui_WAPSwitchImg, &ui_img_switch_symbol_png);
    lv_obj_set_width(ui_WAPSwitchImg, LV_SIZE_CONTENT);   /// 52
    lv_obj_set_height(ui_WAPSwitchImg, LV_SIZE_CONTENT);    /// 58
    lv_obj_set_x(ui_WAPSwitchImg, 0);
    lv_obj_set_y(ui_WAPSwitchImg, -20);
    lv_obj_set_align(ui_WAPSwitchImg, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_WAPSwitchImg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_WAPSwitchImg, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_img_recolor(ui_WAPSwitchImg, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_img_recolor_opa(ui_WAPSwitchImg, 50, LV_PART_MAIN | LV_STATE_PRESSED);

    ui_UAPFanImg = lv_img_create(ui_UiArcPage);
    lv_img_set_src(ui_UAPFanImg, &ui_img_fan_symbol_png);
    lv_obj_set_width(ui_UAPFanImg, LV_SIZE_CONTENT);   /// 55
    lv_obj_set_height(ui_UAPFanImg, LV_SIZE_CONTENT);    /// 53
    lv_obj_set_x(ui_UAPFanImg, -30);
    lv_obj_set_y(ui_UAPFanImg, -20);
    lv_obj_set_align(ui_UAPFanImg, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_add_flag(ui_UAPFanImg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_UAPFanImg, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_img_recolor(ui_UAPFanImg, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_img_recolor_opa(ui_UAPFanImg, 50, LV_PART_MAIN | LV_STATE_PRESSED);

    ui_UAPtittle = lv_label_create(ui_UiArcPage);
    lv_obj_set_width(ui_UAPtittle, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_UAPtittle, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_UAPtittle, 20);
    lv_obj_set_y(ui_UAPtittle, 20);
    lv_label_set_text(ui_UAPtittle, "智能空调");
    lv_obj_set_style_text_color(ui_UAPtittle, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_UAPtittle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_UAPtittle, &ui_font_PingFangCN32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Image2 = lv_img_create(ui_UiArcPage);
    lv_img_set_src(ui_Image2, &ui_img_kongtiao_bg_png);
    lv_obj_set_width(ui_Image2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Image2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Image2, 0);
    lv_obj_set_y(ui_Image2, -30);
    lv_obj_set_align(ui_Image2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image2, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Image2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Label2 = lv_label_create(ui_UiArcPage);
    lv_obj_set_width(ui_Label2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label2, 4);
    lv_obj_set_y(ui_Label2, -20);
    lv_obj_set_align(ui_Label2, LV_ALIGN_CENTER);
    lv_snprintf(buff, sizeof(buff), "%d", temp);
    lv_label_set_text(ui_Label2, buff);
    lv_obj_set_style_text_color(ui_Label2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label2, &ui_font_Number, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_jianbutton = lv_obj_create(ui_UiArcPage);
    lv_obj_set_width(ui_jianbutton, 58);
    lv_obj_set_height(ui_jianbutton, 50);
    lv_obj_set_x(ui_jianbutton, -98);
    lv_obj_set_y(ui_jianbutton, -28);
    lv_obj_set_align(ui_jianbutton, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_jianbutton, LV_OBJ_FLAG_CHECKABLE);     /// Flags
    lv_obj_clear_flag(ui_jianbutton, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_jianbutton, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_jianbutton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_jianbutton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ui_jianbutton, ui_jianbutton_event, LV_EVENT_CLICKED, NULL);

    ui_jiabutton = lv_obj_create(ui_UiArcPage);
    lv_obj_set_width(ui_jiabutton, 58);
    lv_obj_set_height(ui_jiabutton, 50);
    lv_obj_set_x(ui_jiabutton, 99);
    lv_obj_set_y(ui_jiabutton, -27);
    lv_obj_set_align(ui_jiabutton, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_jiabutton, LV_OBJ_FLAG_CHECKABLE);     /// Flags
    lv_obj_clear_flag(ui_jiabutton, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_jiabutton, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_jiabutton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_jiabutton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ui_jiabutton, ui_jiabutton_event, LV_EVENT_CLICKED, NULL);

}