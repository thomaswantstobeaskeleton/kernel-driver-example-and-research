#pragma once
// Include D3D11 before any other headers that might use it
#define INITGUID
#include <d3d11.h>
#include "colors_widgets.h"

namespace settings {
    inline ImVec2 size_menu = ImVec2(750, 670);
    inline ImVec2 size_watermark = ImVec2(479, 50);
    inline ImVec2 size_preview = ImVec2(300, 400);

    namespace exploits {
        inline bool Spinbot = false;
        inline bool FOVChanger = false;
        inline float FOVVALUE = 90.0f;
        inline bool Carfly = false;
        inline bool SpeedHack = false;
        inline bool Spectate = false;
        inline bool BulletTP = false;
        inline float SpinbotSpeed = 5.0f;
    }

    namespace visuals {
        inline int box_type = 0;
        inline bool filled_box = false;
        inline bool outline_box = false;
        inline bool outline_corner_box = false;
        inline bool corner_box_filled = false;
        inline int picture_esp_type = 0; // 0=None, 1=Charlie, 2=Ice(Box)
        inline bool charlie = false; // Legacy - kept for compatibility
    }

    namespace spectate {
        inline bool active = false;
        inline uintptr_t original_pawn = 0;
        inline uintptr_t target_pawn = 0;
        inline bool has_teleported = false;
        inline fvector last_target_position = fvector(0, 0, 0);
        inline uint8_t original_movement_mode = 0;
        inline uint8_t original_pawn_motion = 0;
        inline uint8_t original_controller_motion = 0;
        inline fvector original_mesh_values = fvector(0, 0, 0);
        inline bool mesh_values_stored = false;
    }

    namespace bullet_tp {
        inline bool auto_tp = false;
        inline bool tp_to_head = true;
        inline float tp_delay = 0.1f;
        inline bool ghost_mode = false;
        inline uintptr_t last_target = 0;
        inline fvector last_target_pos = fvector(0, 0, 0);
        inline bool has_shot = false;
    }
}

//namespace misc {
//
//    inline int tab_count, active_tab_count = 0;
//
//    inline float anim_tab = 0;
//
//    inline int tab_width = 85;
//
//    inline float child_add, alpha_child = 0;
//
//}

namespace menu {

    inline ImVec4 general_child = ImColor(23, 23, 25);

}

namespace pictures {

    inline ID3D11ShaderResourceView* logo_img = nullptr;
    inline ID3D11ShaderResourceView* aim_img = nullptr;
    inline ID3D11ShaderResourceView* misc_img = nullptr;
    inline ID3D11ShaderResourceView* visual_img = nullptr;
    inline ID3D11ShaderResourceView* world_img = nullptr;
    inline ID3D11ShaderResourceView* settings_img = nullptr;
    inline ID3D11ShaderResourceView* pen_img = nullptr;
    inline ID3D11ShaderResourceView* keyboard_img = nullptr;
    inline ID3D11ShaderResourceView* input_img = nullptr;
    inline ID3D11ShaderResourceView* wat_logo_img = nullptr;
    inline ID3D11ShaderResourceView* fps_img = nullptr;
    inline ID3D11ShaderResourceView* player_img = nullptr;
    inline ID3D11ShaderResourceView* time_img = nullptr;

}

namespace fonts {

    inline ImFont* medium;
    inline ImFont* semibold;
    inline ImFont* poppins_medium;

    inline ImFont* logo;
    inline ImFont* inter_bold_font;

    inline ImFont* inter_bold_font2;

    inline ImFont* inter_bold_font3;

    inline ImFont* inter_bold_font4;

    inline ImFont* inter_font_b;

    inline ImFont* combo_icon_font;

    inline ImFont* weapon_font;

}


//