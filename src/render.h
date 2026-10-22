#ifndef DC_RENDER_H
#define DC_RENDER_H

#include "assets.h"
#include "camera.h"
#include "level.h"
#include "player.h"

void Render_Game(const Assets *assets, const Level *level, const GameCamera *camera,
                 const Player *player, bool debug_collision);

#endif
