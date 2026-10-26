#include "renderer/NativeSnapshotStats.hpp"
#include <cassert>
#include <iostream>
int main() {
    using namespace chlorium::snapshot;
    Stats s;s.active=1;s.frames=300;s.sprites=42;
    TransferStatsV9 t;t.uploads=80357;t.nativeFallbacks=18;t.busyFallbacks=18;
    assert(validSubmissionStats(s,true,nullptr,false));
    s.fallbacks=18;
    assert(!validSubmissionStats(s,true,&t,false));
    assert(validSubmissionStats(s,true,&t,true));
    assert(!validSubmissionStats(s,true,nullptr,true));
    assert(!validSubmissionStats(s,false,&t,true));
    ++s.fallbacks;assert(!validSubmissionStats(s,true,&t,true));--s.fallbacks;
    ++t.busyFallbacks;assert(!validSubmissionStats(s,true,&t,true));--t.busyFallbacks;
    t.uploads=0;assert(!validSubmissionStats(s,true,&t,true));t.uploads=1;
    t.version=8;assert(!validSubmissionStats(s,true,&t,true));t.version=9;
    s.active=0;assert(!validSubmissionStats(s,true,&t,true));s.active=1;
    s.frames=0;assert(!validSubmissionStats(s,true,&t,true));s.frames=1;
    s.sprites=0;assert(!validSubmissionStats(s,true,&t,true));s.sprites=1;
    s.version=0;assert(!validSubmissionStats(s,true,&t,true));
    std::cout<<"PASS: 13 native/hybrid policy checks; unsupported or unaccounted fallback rejected\n";
}
