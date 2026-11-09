#pragma once
class PlayLayer;
namespace chlorium::research::grouptrace {
void begin(PlayLayer* play,bool isolated)noexcept;
void end()noexcept;
}
