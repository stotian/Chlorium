#pragma once
class PlayLayer;
namespace chlorium::research::fieldtrace {
enum Reason:unsigned {StartFailed=2,Overflow=4,OwnerLost=8,IncompleteWindow=16,OutputFailed=32,ObserverFault=64};
struct Stats {unsigned version=69,active=0,complete=0,reasons=0,frames=0,rows=0,objects=0,failure=0;};
void begin(PlayLayer* play,bool closed);
void end();
}
