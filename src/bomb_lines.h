// src/bomb_lines.h —— 四个角色的专属语音(tools/gen_lines.py 生成,勿手改)。下标 = bomb_fighter_t。
// 格式同 Coopa OS 内置台词(16 kHz IMA ADPCM),用 sfx_play_line 播放。台词:

// PICK_KID: 库巴来啦！
// PICK_DAD: 放马过来吧！
// PICK_GRANNY: 看奶奶的厉害！
// PICK_GRANDPA: 爷爷来陪你耍。
// SPECIAL_KID: 弹弓，发射！
// SPECIAL_DAD: 火箭快递，使命必达！
// SPECIAL_GRANNY: 听奶奶念叨念叨！
// SPECIAL_GRANDPA: 戴上老花镜，看清楚喽！
// SUPER_KID: 影分身之术！
// SUPER_DAD: 铁笼！给我关起来！
// SUPER_GRANNY: 锅盖神功！
// SUPER_GRANDPA: 太极推手——走你！
// WIN_KID: 耶！我赢啦！
// WIN_DAD: 谁也别想赢过我！
// WIN_GRANNY: 服不服？
// WIN_GRANDPA: 姜还是老的辣！

#pragma once

#include "kit_lines.h"

extern const kit_line_t BL_PICK[4], BL_SPECIAL[4], BL_SUPER[4], BL_WIN[4];
