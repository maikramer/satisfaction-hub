#pragma once

// Funções internas do ui_driver exportadas para uso em screens
void lvgl_lock();
void lvgl_unlock();

// Funções para resetar timeout automático
void reset_password_timeout();
void reset_config_timeout();

