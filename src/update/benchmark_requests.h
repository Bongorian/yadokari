#pragma once
namespace groove { namespace update {
// One pending non-persistent test mode; only start after control reply drains.
class BenchmarkRequests {
public:
 bool request(unsigned kind){if(pending_ || kind<1 || kind>26)return false;pending_=kind;return true;}
 bool take(unsigned kind,bool reply_pending){if(reply_pending || !kind || pending_!=kind)return false;pending_=0;return true;}
private:unsigned pending_=0;
};
}}
